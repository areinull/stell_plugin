#include "StelProjector.hpp"
#include "StelPainter.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelFileMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "StelObjectMgr.hpp"
#include "StelTextureMgr.hpp"
#include "StelUtils.hpp"
#include "SatTrajMgr.hpp"
#include "SatTrajDialog.hpp"
#include "SimpleTraj.hpp"
#include "MeasTraj.hpp"
#include "AntTraj.hpp"
#include "GoodSample.hpp"

#include <QtOpenGL/QtOpenGL>
#include <QtCore/QTimer>
#include <vu_tools/vu_tools.h>
#include <coord_conv/CoordConv.h>
#include <locale.h>

StelModule* SatTrajMgrStelPluginInterface::getStelModule() const
{
  return new SatTrajMgr();
}

StelPluginInfo SatTrajMgrStelPluginInterface::getPluginInfo() const
{
  // Allow to load the resources when used as a static plugin
  Q_INIT_RESOURCE(SatTrajMgr);

  StelPluginInfo info;
  info.id = "SatTrajMgr";
  info.displayedName = "Satellite Trajectory";
  info.authors = "";
  info.contact = "";
  info.description = "Provides satellite trajectories graphics";
  return info;
}

Q_EXPORT_PLUGIN2(SatTrajMgr, SatTrajMgrStelPluginInterface)


SatTrajMgr::SatTrajMgr()
    : azAdj(0)
    , zaAdj(0)
    , azIncr(360)
    , zaIncr(360)
    , syncPeriod(2.)
    , antExtr(false)
    , antExtrK(1.2)
    , antExtrTime(baseUpdTime*antExtrK)
    , changeDecimal(false)
    , gWidth(450)
    , gHeight(64)
    , enableGoodSamples(false)
    , enableShm(false)
    , pMeasColor(new QColor)
    , pTdColor(new QColor)
    , pRefColor(new QColor)
    , pEstColor(new QColor)
    , pDebugColor(new QColor)
    , pAntColor(new QColor)
    , flagShowSatTraj(false)
    , settings(NULL)
    , messageTimer(NULL)
    , messageFont("Monospace")
    , pxmapGlow(NULL)
    , pxmapOnIcon(NULL)
    , pxmapOffIcon(NULL)
    , resetOffIcon(NULL)
    , resetOnIcon(NULL)
    , syncOffIcon(NULL)
    , syncOnIcon(NULL)
    , toolbarButton(NULL)
    , tbbReset(NULL)
    , tbbSync(NULL)
    , ntpSync(NULL)
    , secProcInfo(NULL)
    , dbIsUp(true)
    , antennaSelected(false)
    , gotoSet(false)
    , baseUpdTime(1.)
    , dbThread(0)
    , lastGoodMeasID(-1)
    , lineSpacing(0)
{
  setObjectName("SatTrajMgr");
  configDialog = new SatTrajDialog();
  if (*localeconv()->decimal_point == ',')
      changeDecimal = true;
  mysql_library_init(0, NULL, NULL);
}

SatTrajMgr::~SatTrajMgr()
{
  delete configDialog;
  mysql_library_end();
}

double SatTrajMgr::getCallOrder(StelModuleActionName actionName) const
{
  if (actionName == StelModule::ActionDraw)
    return StelApp::getInstance().getModuleMgr().getModule("LandscapeMgr")->
           getCallOrder(actionName)+1.;
  else if (actionName == StelModule::ActionHandleMouseClicks)
    return StelApp::getInstance().getModuleMgr().getModule("StelMovementMgr")->
           getCallOrder(actionName)-1.;
  return 0;
}

void SatTrajMgr::init()
{
  settings = StelApp::getInstance().getSettings();

  try
  {
    // If no settings in the main config file, create with defaults
    if (!settings->childGroups().contains("SatTrajMgr"))
      restoreDefaultConfigIni();
    // populate settings from main config file
    readSettingsFromConfig();

    loadTex();

    // add gui action
    StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
    Q_ASSERT(gui);
    QString groupName("SatTraj Key Bindings");
    gui->addGuiActions("actionShow_SatTrajMgr_ConfigDialog",
                       "Satellite Trajectories Config Dialog","",groupName);
    gui->addGuiActions("actionShow_SatTrajMgr", "Satellite Trajectories",
                       "", groupName);
    gui->addGuiActions("actionAdriveAz+", "Increase azimuth adjustment",
                       "Shift+D", groupName, false, true);
    gui->addGuiActions("actionAdriveAz-", "Decrease azimuth adjustment",
                       "Shift+A", groupName, false, true);
    gui->addGuiActions("actionAdriveZa+", "Increase zenith angle adjustment",
                       "Shift+W", groupName, false, true);
    gui->addGuiActions("actionAdriveZa-", "Decrease zenith angle adjustment",
                       "Shift+S", groupName, false, true);
    gui->addGuiActions("actionResetAdj", "Set angle adjustments to zero",
                       "", groupName, false, false);
    gui->addGuiActions("actionNtptimedSync",
                       "Syncronize model time with ntptimed", "", groupName);
    gui->getGuiActions("actionShow_SatTrajMgr")->setChecked(flagShowSatTraj);
    gui->getGuiActions("actionNtptimedSync")->setChecked(false);
    connect(gui->getGuiActions("actionShow_SatTrajMgr_ConfigDialog"),
            SIGNAL(toggled(bool)), configDialog, SLOT(setVisible(bool)));
    connect(gui->getGuiActions("actionShow_SatTrajMgr"), SIGNAL(toggled(bool)),
            this, SLOT(enableSatTrajMgr(bool)));
    connect(gui->getGuiActions("actionAdriveAz+"), SIGNAL(triggered()),
            this, SLOT(adriveAzIncr()));
    connect(gui->getGuiActions("actionAdriveAz-"), SIGNAL(triggered()),
            this, SLOT(adriveAzDecr()));
    connect(gui->getGuiActions("actionAdriveZa+"), SIGNAL(triggered()),
            this, SLOT(adriveZaIncr()));
    connect(gui->getGuiActions("actionAdriveZa-"), SIGNAL(triggered()),
            this, SLOT(adriveZaDecr()));
    connect(gui->getGuiActions("actionResetAdj"), SIGNAL(triggered()),
            this, SLOT(resetAdj()));
    connect(gui->getGuiActions("actionNtptimedSync"), SIGNAL(toggled(bool)),
            this, SLOT(changeSyncState(bool)));

    // Gui toolbar button
    QString buttonGroup("070-test");
    pxmapGlow = new QPixmap(":/stell_plug/glow32x32.png");
    pxmapOnIcon = new QPixmap(":/stell_plug/bt_sattraj_on.png");
    pxmapOffIcon = new QPixmap(":/stell_plug/bt_sattraj_off.png");
    toolbarButton = new StelButton(NULL, *pxmapOnIcon, *pxmapOffIcon,
                       *pxmapGlow, gui->getGuiActions("actionShow_SatTrajMgr"));
    gui->getButtonBar()->addButton(toolbarButton, buttonGroup);
    resetOffIcon = new QPixmap(":/stell_plug/Zero.png");
    resetOnIcon = new QPixmap(":/stell_plug/ZeroOn.png");
    tbbReset = new StelButton(NULL, *resetOnIcon, *resetOffIcon, *pxmapGlow,
                              gui->getGuiActions("actionResetAdj"));
    gui->getButtonBar()->addButton(tbbReset, buttonGroup);
    syncOffIcon = new QPixmap(":/stell_plug/ntpSyncOff.png");
    syncOnIcon = new QPixmap(":/stell_plug/ntpSyncOn.png");
    tbbSync = new StelButton(NULL, *syncOnIcon, *syncOffIcon, *pxmapGlow,
                             gui->getGuiActions("actionNtptimedSync"));
    tbbSync->setEnabled(false);
    gui->getButtonBar()->addButton(tbbSync, buttonGroup);
  }
  catch (std::runtime_error &e)
  {
    qWarning() << "SatTrajMgr::init error: " << e.what();
    return;
  }

  GETSTELMODULE(StelObjectMgr)->registerStelObjectMgr(this);

  // MYSQL initialization
  pthread_mutex_init(&dbLock, NULL);
  dbIsUp = initMysql(mysql);

  updAzAdj();
  updZaAdj();

  pthread_cond_init(&dbCv, NULL);
  if (pthread_create(&dbThread, NULL, callRoutine, this))
  {
      qWarning() << "pthread_create() " << strerror(errno);
      return;
  }

  // Set up message timer
  messageTimer = new QTimer(this);
  messageTimer->setInterval(20000);
  messageTimer->setSingleShot(true);
  connect(messageTimer, SIGNAL(timeout()), this, SLOT(clearMessage()));

  if (flagShowSatTraj)
    initTraj();
}

void SatTrajMgr::changeSyncState(bool b)
{
    if (b)
    {
        if (ntpSync)
        {
            qDebug() << "Warning: ntpSync still exists";
            delete ntpSync;
        }
        ntpSync = new NtpSync(this, (int)(syncPeriod*1e3));
    }
    else
    {
        delete ntpSync;
        ntpSync = NULL;
    }
}

void SatTrajMgr::loadTex(void)
{
    texPointer = StelApp::getInstance().getTextureManager().
               createTexture("textures/pointeur5.png");
    arrowTex = StelApp::getInstance().getTextureManager().
               createTexture(":/stell_plug/arrow.png");
    GoodSample::hintTexture = StelApp::getInstance().getTextureManager().
               createTexture(":/stell_plug/sample_hint.png");
}

void SatTrajMgr::initTraj(void)
{
    GenObjP traj;
    pMeasTraj = GenTrajP(new MeasTraj(QString("Measurement"), "",
                                     0, *this, pMeasColor));
    if (pMeasTraj && pMeasTraj->isInit())
    {
        traj = pMeasTraj;
        objects.append(traj);
    }
    pTdTraj = GenTrajP(new SimpleTraj(QString("Target designation"), ":/stell_plug/triangle.png",
                               1, *this, pTdColor));
    if (pTdTraj && pTdTraj->isInit())
    {
        traj = pTdTraj;
        objects.append(traj);
    }
    pRefTraj = GenTrajP(new SimpleTraj(QString("Reference"), ":/stell_plug/sat_hint_1.png",
                               2, *this, pRefColor));
    if (pRefTraj && pRefTraj->isInit())
    {
        traj = pRefTraj;
        objects.append(traj);
    }
    pEstTraj = GenTrajP(new SimpleTraj(QString("Estimation"), ":/stell_plug/sat_hint_1.png",
                               3, *this, pEstColor));
    if (pEstTraj && pEstTraj->isInit())
    {
        traj = pEstTraj;
        objects.append(traj);
    }
    pDebugTraj = GenTrajP(new SimpleTraj(QString("Debug"), ":/stell_plug/sat_hint_1.png",
                               4, *this, pDebugColor));
    if (pDebugTraj && pDebugTraj->isInit())
    {
        traj = pDebugTraj;
        objects.append(traj);
    }
    pAntTraj = GenTrajP(new AntTraj(QString("Antenna"), ":/stell_plug/square.png",
                               5, *this, pAntColor));
    if (pAntTraj && pAntTraj->isInit())
    {
        traj = pAntTraj;
        objects.append(traj);
    }
}

void SatTrajMgr::resetAdj(void)
{
    if (!flagShowSatTraj)
        return;
    azAdj = 0;
    zaAdj = 0;
    updAzAdj();
    updZaAdj();
    qDebug()<<"resetAdj()";
}

void SatTrajMgr::adriveAzIncr(void)
{
    if (!flagShowSatTraj || !antennaSelected)
        return;
    azAdj += azIncr;
    updAzAdj();
    qDebug()<<"SatTrajMgr::adriveAzIncr() azAdj = "<<azAdj;
}

void SatTrajMgr::adriveAzDecr(void)
{
    if (!flagShowSatTraj || !antennaSelected)
        return;
    azAdj -= azIncr;
    updAzAdj();
    qDebug()<<"SatTrajMgr::adriveAzDecr() azAdj = "<<azAdj;
}

void SatTrajMgr::updAzAdj(void)
{
    if (!dbIsUp)
        return;
    char buf[1024];
    snprintf(buf, 1024, "UPDATE adrive_params SET val='%d' WHERE name='Az_adj'",
             azAdj);
    if (mysql_query(&mysql,buf))
    {
        snprintf(buf,1024,"[%s:%d] Error: can't send query to server:\n\t",
                 _FILE_,__LINE__);
        std::string err(buf);
        err+=mysql_error(&mysql);
        throw std::runtime_error(err);
    }
    qDebug()<<"updAzAdj()";
}

void SatTrajMgr::adriveZaIncr(void)
{
    if (!flagShowSatTraj || !antennaSelected)
        return;
    zaAdj += zaIncr;
    updZaAdj();
    qDebug()<<"SatTrajMgr::adriveZaIncr() zaAdj = "<<zaAdj;
}

void SatTrajMgr::adriveZaDecr(void)
{
    if (!flagShowSatTraj || !antennaSelected)
        return;
    zaAdj -= zaIncr;
    updZaAdj();
    qDebug()<<"SatTrajMgr::adriveZaDecr() zaAdj = "<<zaAdj;
}

void SatTrajMgr::updZaAdj(void)
{
    if (!dbIsUp)
        return;
    char buf[1024];
    snprintf(buf, 1024, "UPDATE adrive_params SET val='%d' WHERE name='Za_adj'",
             zaAdj);
    if (mysql_query(&mysql, buf))
    {
        snprintf(buf, 1024, "[%s:%d] Error: can't send query to server:\n\t",
                 _FILE_, __LINE__);
        std::string err(buf);
        err+=mysql_error(&mysql);
        throw std::runtime_error(err);
    }
    qDebug()<<"updZaAdj()";
}

void SatTrajMgr::setAzIncr(int val)
{
    azIncr = (val<0)? 0: val;
    qDebug()<<"SatTrajMgr::setAzIncr() azIncr = "<<azIncr;
}

void SatTrajMgr::setZaIncr(int val)
{
    zaIncr = (val<0)? 0: val;
    qDebug()<<"SatTrajMgr::setZaIncr() zaIncr = "<<zaIncr;
}

bool SatTrajMgr::configureGui(bool show)
{
  if (show)
  {
    StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
    gui->getGuiActions("actionShow_SatTrajMgr_ConfigDialog")->setChecked(true);
  }
  return true;
}

void SatTrajMgr::restoreDefaultConfigIni()
{
  settings->beginGroup("SatTrajMgr");

  // delete all existing Satellite settings...
  settings->remove("");

  settings->setValue("text_color", "0,0.5,1");
  settings->setValue("line0_color", "0.5,0,1");
  settings->setValue("line1_color", "0,0,1");
  settings->setValue("line2_color", "0,1,0");
  settings->setValue("line3_color", "1,0,0");
  settings->setValue("line4_color", "1,0.5,0");
  settings->setValue("line5_color", "0,1,1");
  settings->setValue("hint_font_size", 14);
  settings->setValue("db_upd_time", 3.);
  settings->setValue("db_name", "test");
  settings->setValue("db_user", "root");
  settings->setValue("db_pass", "888");
  settings->setValue("db_host", "localhost");
  settings->setValue("db_port", 3306);
  settings->setValue("db_table", "SatelliteTrack");
  settings->setValue("timeWindow", 240);
  settings->setValue("tw_sample", 5.f);
  settings->setValue("sync_period", 2.f);

  settings->endGroup();
}

void SatTrajMgr::readSettingsFromConfig()
{
  settings->beginGroup("SatTrajMgr");
  Vec3f tmp;
  tmp = StelUtils::strToVec3f(settings->value("text_color", "0,0.5,1").toString());
  textColor.setRgbF(tmp[0], tmp[1], tmp[2]);
  tmp = StelUtils::strToVec3f(settings->value("line0_color", "0.5,0,1").toString());
  pMeasColor->setRgbF(tmp[0], tmp[1], tmp[2]);
  tmp = StelUtils::strToVec3f(settings->value("line1_color", "0,0,1").toString());
  pTdColor->setRgbF(tmp[0], tmp[1], tmp[2]);
  tmp = StelUtils::strToVec3f(settings->value("line2_color", "0,1,0").toString());
  pRefColor->setRgbF(tmp[0], tmp[1], tmp[2]);
  tmp = StelUtils::strToVec3f(settings->value("line3_color", "1,0,0").toString());
  pEstColor->setRgbF(tmp[0], tmp[1], tmp[2]);
  tmp = StelUtils::strToVec3f(settings->value("line4_color", "1,0.5,0").toString());
  pDebugColor->setRgbF(tmp[0], tmp[1], tmp[2]);
  tmp = StelUtils::strToVec3f(settings->value("line5_color", "0,1,1").toString());
  pAntColor->setRgbF(tmp[0], tmp[1], tmp[2]);
  messageFont.setPixelSize(settings->value("hint_font_size", 14).toInt());
  lineSpacing = QFontMetrics(messageFont).lineSpacing();
  baseUpdTime = settings->value("db_upd_time", 1.).toDouble();
  antExtrTime = baseUpdTime*antExtrK;
  database = settings->value("db_name", "test").toString().toStdString();
  user = settings->value("db_user", "root").toString().toStdString();
  pass = settings->value("db_pass", "888").toString().toStdString();
  host = settings->value("db_host", "localhost").toString().toStdString();
  port = settings->value("db_port", 3306).toUInt();
  tableName = settings->value("db_table", "SatelliteTrack").toString().toStdString();
  timeWindow = settings->value("timeWindow", 240).toUInt();
  timeWindowSamples = settings->value("tw_sample", 5.f).toDouble();
  syncPeriod = settings->value("sync_period", 2.f).toDouble();

  settings->endGroup();
}

void SatTrajMgr::deinit(void)
{
  settings->beginGroup("SatTrajMgr");
  settings->setValue("text_color",QString("%1,%2,%3").arg(textColor.redF())
                                                     .arg(textColor.greenF())
                                                     .arg(textColor.blueF()));
  settings->setValue("line0_color",QString("%1,%2,%3").arg(pMeasColor->redF())
                                                      .arg(pMeasColor->greenF())
                                                      .arg(pMeasColor->blueF()));
  settings->setValue("line1_color",QString("%1,%2,%3").arg(pTdColor->redF())
                                                      .arg(pTdColor->greenF())
                                                      .arg(pTdColor->blueF()));
  settings->setValue("line2_color",QString("%1,%2,%3").arg(pRefColor->redF())
                                                      .arg(pRefColor->greenF())
                                                      .arg(pRefColor->blueF()));
  settings->setValue("line3_color",QString("%1,%2,%3").arg(pEstColor->redF())
                                                      .arg(pEstColor->greenF())
                                                      .arg(pEstColor->blueF()));
  settings->setValue("line4_color",QString("%1,%2,%3").arg(pDebugColor->redF())
                                                      .arg(pDebugColor->greenF())
                                                      .arg(pDebugColor->blueF()));
  settings->setValue("line5_color",QString("%1,%2,%3").arg(pAntColor->redF())
                                                      .arg(pAntColor->greenF())
                                                      .arg(pAntColor->blueF()));
  settings->setValue("db_upd_time",baseUpdTime);
  settings->setValue("db_name",QString(database.c_str()));
  settings->setValue("db_user",QString(user.c_str()));
  settings->setValue("db_pass",QString(pass.c_str()));
  settings->setValue("db_host",QString(host.c_str()));
  settings->setValue("db_port",port);
  settings->setValue("db_table",QString(tableName.c_str()));
  settings->setValue("timeWindow",timeWindow);
  settings->setValue("tw_sample", timeWindowSamples);
  settings->setValue("sync_period", syncPeriod);
  settings->endGroup();
  settings = NULL;

  delete ntpSync;
  delete secProcInfo;
  if (dbThread)
  {
      pthread_mutex_lock(&dbLock);
      pthread_cancel(dbThread);
      pthread_mutex_unlock(&dbLock);
      pthread_join(dbThread, NULL);
  }
  pthread_cond_destroy(&dbCv);
  pthread_mutex_destroy(&dbLock);
  if (dbIsUp)
  {
    mysql_close(&mysql);
    dbIsUp = false;
  }
  objects.clear(); // do not call deinitTraj (mutex related issue)
  GoodSample::hintTexture.clear();
  lastGoodMeasID = -1;
  delete messageTimer;
  delete pxmapGlow;
  delete pxmapOnIcon;
  delete pxmapOffIcon;
  delete resetOffIcon;
  delete resetOnIcon;
  delete syncOffIcon;
  delete syncOnIcon;
}

void SatTrajMgr::update(double deltaTime)
{
    static double time2AdjUpd = baseUpdTime;
    static double time2TrajUpd = baseUpdTime/6.;
    if (messageFader || messageFader.getInterstate() > 0.f)
        messageFader.update(static_cast<int>(deltaTime*1000));
    if (adjInfoFader || adjInfoFader.getInterstate() > 0.f)
        adjInfoFader.update(static_cast<int>(deltaTime*1000));
    if (flagShowSatTraj && dbIsUp)
    {
        time2AdjUpd -= deltaTime;
        time2TrajUpd -= deltaTime;
        if (time2AdjUpd < 0.)
        {
            getAdj();
            time2AdjUpd = baseUpdTime;
        }
        if (time2TrajUpd < 0.)
        {
            if (!pthread_mutex_trylock(&dbLock))
            {
                pthread_cond_broadcast(&dbCv);
                pthread_mutex_unlock(&dbLock);
                time2TrajUpd = baseUpdTime/6.;
            }
        }
        if (gotoSet)
            gotoFunc(deltaTime);
        if (secProcInfo)
        {
            secProcInfo->update(deltaTime);
        }
    }
}

void SatTrajMgr::getAdj(void)
{
    char buf[1024];
    MYSQL_RES *pRes;
    MYSQL_ROW row;
    snprintf(buf,1024,"SELECT name,val FROM adrive_params WHERE name='Az_adj' "
                      "OR name='Za_adj'");
    if (mysql_query(&mysql,buf))
    {
        char buf[1024];
        snprintf(buf,1024,"[%s:%d] Error: can't send query to server:\n\t",
                 _FILE_,__LINE__);
        std::string err(buf);
        err+=mysql_error(&mysql);
        throw std::runtime_error(err);
    }
    pRes = mysql_use_result(&mysql);
    while ((row = mysql_fetch_row(pRes)))
    {
        if (!strcmp(row[0], "Az_adj"))
        {
            azAdj = atoi(row[1]);
            continue;
        }
        if (!strcmp(row[0], "Za_adj"))
        {
            zaAdj = atoi(row[1]);
            continue;
        }
    }
    mysql_free_result(pRes);
}

void SatTrajMgr::enableSatTrajMgr(bool b)
{
    flagShowSatTraj = b;
    messageFader = b;
    adjInfoFader = b;
    if (b)
    {
        tbbSync->setEnabled(true);
        messageTimer->start();
        if (!dbIsUp)    // try to connect to DB again
             dbIsUp = initMysql(mysql);
        if (!objects.empty())
            deinitTraj();
        if (dbIsUp)
        {
            if (secProcInfo)
            {
                qDebug() << "Warning: secProcInfo still exists";
                delete secProcInfo;
            }
            secProcInfo = new SecProcInfo(this, 2.);
        }
        loadTex();
        initTraj();
    }
    else
    {
        if (GETSTELMODULE(StelObjectMgr)->getWasSelected())
        {
            QList<StelObjectP> newSelected =
                     GETSTELMODULE(StelObjectMgr)->getSelectedObject("AntTraj");
            if (!newSelected.isEmpty())
            {
                GETSTELMODULE(StelObjectMgr)->unSelect();
            }
            newSelected = GETSTELMODULE(StelObjectMgr)->getSelectedObject("SimpleTraj");
            if (!newSelected.isEmpty())
            {
                GETSTELMODULE(StelObjectMgr)->unSelect();
            }
            newSelected = GETSTELMODULE(StelObjectMgr)->getSelectedObject("MeasTraj");
            if (!newSelected.isEmpty())
            {
                GETSTELMODULE(StelObjectMgr)->unSelect();
            }
            newSelected = GETSTELMODULE(StelObjectMgr)->getSelectedObject("GoodSample");
            if (!newSelected.isEmpty())
            {
                GETSTELMODULE(StelObjectMgr)->unSelect();
            }
        }
        StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
        gui->getGuiActions("actionNtptimedSync")->setChecked(false);
        tbbSync->setEnabled(false);
        tbbSync->setChecked(false);
        if (secProcInfo)
        {
            delete secProcInfo;
            secProcInfo = NULL;
        }
        if (dbIsUp)
        {
            mysql_close(&mysql);
            dbIsUp = false;
        }
        setGotoPoint(false);
        deinitTraj();
    }
}

void SatTrajMgr::deinitTraj()
{
    pthread_mutex_lock(&dbLock);
    objects.clear();
    pMeasTraj.clear();
    pTdTraj.clear();
    pRefTraj.clear();
    pEstTraj.clear();
    pDebugTraj.clear();
    pAntTraj.clear();
    lastGoodMeasID = -1;
    pthread_mutex_unlock(&dbLock);
}

void SatTrajMgr::clearMessage()
{
  messageFader = false;
}

void SatTrajMgr::draw(StelCore* core)
{
    StelProjectorP prj = core->getProjection(StelCore::FrameAltAz);
    StelPainter painter(prj);

    if (messageFader.getInterstate() > 0.f)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        painter.setColor(textColor.redF(), textColor.greenF(),
                         textColor.blueF(), messageFader.getInterstate());
        painter.setFont(messageFont);
        painter.drawText(83, 95+6.5f*lineSpacing, "SatTrajMgr Controls:");
        painter.drawText(83, 95+5*lineSpacing,
                         "Increase azimuth      - Shift+D");
        painter.drawText(83, 95+4*lineSpacing,
                         "Decrease azimuth      - Shift+A");
        painter.drawText(83, 95+3*lineSpacing,
                         "Increase zenith angle - Shift+W");
        painter.drawText(83, 95+2*lineSpacing,
                         "Decrease zenith angle - Shift+S");
        painter.drawText(83, 95+1*lineSpacing,
                         "Set/unset goto point  - Ctrl+LBM/RMB");
        painter.drawText(83, 95,
                         "NOTE: Antenna must be selected to control it");
    }

    if (adjInfoFader.getInterstate() > 0.f)
        drawAdjInfo(core, painter);

    if (ntpSync)
        ntpSync->draw(painter);

    if (secProcInfo)
        secProcInfo->draw(painter);

    if (!flagShowSatTraj)
        return;

    if (!dbIsUp)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        painter.setColor(1.f,0.f,0.f);
        painter.setFont(messageFont);
        painter.drawText(prj->getViewportCenter()[0] - 100,
                         prj->getViewportCenter()[1],
                        "Cannot connect to MYSQL database");
        return;
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);

    foreach (const GenObjP &traj, objects)
    {
        if (traj && traj->isInit())
            traj->draw(this, prj, painter);
    }

    // Draw goto point
    if (gotoSet)
    {
        Vec3d screenpos;
        painter.setColor(1.f,0.f,0.f,0.5f);
        painter.drawGreatCircleArc(pAntTraj->getAltAzPos(), gotoPoint);
        if (!prj->project(gotoPoint, screenpos))
            goto gotoSetEnd;
        if (!prj->checkInViewport(screenpos))
        {
            Vec2f xy_center = prj->getViewportCenter();
            painter.enableTexture2d(true);
            arrowTex->bind();
            Vec2f tmp;
            tmp[0] = screenpos[0] - xy_center[0];
            tmp[1] = screenpos[1] - xy_center[1];
            tmp.normalize();
            float rot = acos(tmp[1])*180./M_PI;
            if (tmp[0] >= 0.f)
                rot *= -1.f;
            const float r = 64.f;
            painter.drawSprite2dMode(xy_center[0] + r*tmp[0],
                                     xy_center[1] + r*tmp[1], 16.f, rot);
            painter.enableTexture2d(false);
        }
        else
        {
            painter.setColor(1.f,0.f,0.f);
            painter.drawCircle(screenpos[0], screenpos[1], 5.f);
        }
    }
gotoSetEnd:

    // Draw pointer
    if (GETSTELMODULE(StelObjectMgr)->getWasSelected())
    {
        const QList<StelObjectP> newSelected = GETSTELMODULE(StelObjectMgr)->
                                                getSelectedObject();
        if (!newSelected.isEmpty())
        {
            const StelObjectP obj = newSelected[0];
            if ((obj->getType() == "GoodSample" || obj->getType() == "SimpleTraj" ||
                 obj->getType() == "AntTraj"))
            {
                if (dynamic_cast<GenObject*>(obj.data())->isVisible())
                {
                    if (obj->getType() == "AntTraj")
                    {
                        antennaSelected = true;
                    }
                    Vec3d pos = core->j2000ToAltAz(obj->getJ2000EquatorialPos(core));
                    Vec3d screenpos;

                    // Compute 2D pos and return if outside screen
                    if (!prj->project(pos, screenpos))
                        return;
                    painter.setColor(1.f,0.f,0.3f);
                    texPointer->bind();

                    painter.enableTexture2d(true);

                    // Size on screen
                    float size = obj->getAngularSize(core)*M_PI/180.*
                                prj->getPixelPerRadAtCenter();
                    size += 15.f + 8.f*sin(2.f *
                            StelApp::getInstance().getTotalRunTime());
                    painter.drawSprite2dMode(screenpos[0]-size/2,
                                            screenpos[1]-size/2, 20, 90);
                    painter.drawSprite2dMode(screenpos[0]-size/2,
                                            screenpos[1]+size/2, 20, 0);
                    painter.drawSprite2dMode(screenpos[0]+size/2,
                                            screenpos[1]+size/2, 20, -90);
                    painter.drawSprite2dMode(screenpos[0]+size/2,
                                            screenpos[1]-size/2, 20, -180);

                    painter.enableTexture2d(false);
                }
                else
                {
                    antennaSelected = false;
                    GETSTELMODULE(StelObjectMgr)->unSelect();
                }
            }
        }
        else
        {
            antennaSelected = false;
        }
    }
    else
    {
        antennaSelected = false;
    }
}

void SatTrajMgr::setGotoPoint(bool b)
{
    qDebug() <<"setGotoPoint()"<<b;
    char buf[4096];
    if (b)
    {
        StelCore *core = StelApp::getInstance().getCore();
        StelProjectorP prj = core->getProjection(StelCore::FrameAltAz);
        if (!prj->unProject(gotoXY.x(), gotoXY.y(), gotoPoint))
        {
            qDebug()<<"unProject returned false";
            gotoSet = false;
            return;
        }
        if (!dbIsUp)
            goto finally;
        // Перевод в СК РЛС из внутреннего формата stellarium
        double_point rls;
        double az_inv_s1, az_inv_s2, az_inv_c1, az_inv_c2;
        az_inv_s1 = asin(gotoPoint[1]/sqrt(1.-gotoPoint[2]*gotoPoint[2]));
        if (az_inv_s1 >= 0.)
            az_inv_s2 = M_PI - az_inv_s1;
        else
            az_inv_s2 = -M_PI - az_inv_s1;
        az_inv_c1 = acos(-gotoPoint[0]/sqrt(1.-gotoPoint[2]*gotoPoint[2]));
        az_inv_c2 = -az_inv_c1;
        double diff, min_diff;
        int min_i = 0;
        // i == 0
        min_diff = diff = fabs(az_inv_s1 - az_inv_c1);
        // i == 1
        if ((diff = fabs(az_inv_s1 - az_inv_c2)) < min_diff)
        {
            min_diff = diff;
            min_i = 1;
        }
        // i == 2
        if ((diff = fabs(az_inv_s2 - az_inv_c1)) < min_diff)
        {
            min_diff = diff;
            min_i = 2;
        }
        // i == 3
        if ((diff = fabs(az_inv_s2 - az_inv_c2)) < min_diff)
        {
            min_diff = diff;
            min_i = 3;
        }
        switch (min_i)
        {
            case 0: case 1:
                rls.first = az_inv_s1;
                break;
            case 2: case 3:
                rls.first = az_inv_s2;
                break;
        }
        rls.second = atan(gotoPoint[2]/sqrt(1.-gotoPoint[2]*gotoPoint[2]));
        qDebug()<<"New point RLS:"<<rls.first<<rls.second;
        // Перевод в СК антенны
        int_point ant, ant_cur;
        MYSQL_RES *pRes;
        MYSQL_ROW row;
        snprintf(buf,4096,"SELECT name,val FROM adrive_params WHERE "
                          "name='cur_Az' OR name='cur_Za'");
        if (mysql_query(&mysql,buf))
        {
            snprintf(buf,4096,"[%s:%d] Error: can't send query to server:\n\t",
                     _FILE_,__LINE__);
            std::string err(buf);
            err+=mysql_error(&mysql);
            throw std::runtime_error(err);
        }
        pRes = mysql_use_result(&mysql);
        while ((row = mysql_fetch_row(pRes)))
        {
            if (!strcmp(row[0], "cur_Az"))
            {
                ant_cur.first = atoi(row[1]);
                continue;
            }
            if (!strcmp(row[0], "cur_Za"))
            {
                ant_cur.second = atoi(row[1]);
                continue;
            }
        }
        mysql_free_result(pRes);
        qDebug()<<"Cur point ANT:"<<ant_cur.first<<ant_cur.second;
        ant = radar2antenna(rls, ant_cur);
        new_az = ant.first;
        new_za = ant.second;
        qDebug()<<"New point ANT:"<<ant.first<<ant.second;
        // Set new point in DB
        snprintf(buf, 4096,
                 "UPDATE adrive_params SET val='%d' WHERE name='new_Az'",
                 ant.first);
        if (mysql_query(&mysql,buf))
        {
            snprintf(buf,4096,"[%s:%d] Error: can't send query to server:\n\t",
                     _FILE_,__LINE__);
            std::string err(buf);
            err+=mysql_error(&mysql);
            throw std::runtime_error(err);
        }
        snprintf(buf, 4096,
                 "UPDATE adrive_params SET val='%d' WHERE name='new_Za'",
                 ant.second);
        if (mysql_query(&mysql,buf))
        {
            snprintf(buf,4096,"[%s:%d] Error: can't send query to server:\n\t",
                     _FILE_,__LINE__);
            std::string err(buf);
            err+=mysql_error(&mysql);
            throw std::runtime_error(err);
        }
        snprintf(buf, 4096,
                 "UPDATE adrive_params SET val='2' WHERE name='Mode'");
        if (mysql_query(&mysql,buf))
        {
            snprintf(buf,4096,"[%s:%d] Error: can't send query to server:\n\t",
                     _FILE_,__LINE__);
            std::string err(buf);
            err+=mysql_error(&mysql);
            throw std::runtime_error(err);
        }
    }
    else
    {
        if (!dbIsUp)
            goto finally;
        snprintf(buf, 4096,
                 "UPDATE adrive_params SET val='1' WHERE name='Mode'");
        if (mysql_query(&mysql,buf))
        {
            snprintf(buf,4096,"[%s:%d] Error: can't send query to server:\n\t",
                     _FILE_,__LINE__);
            std::string err(buf);
            err+=mysql_error(&mysql);
            throw std::runtime_error(err);
        }
    }
finally:
    gotoSet = b;
}

void SatTrajMgr::gotoFunc(double delta)
{
//     const double delta_sq = pow(1000. / 3600. / 180. * M_PI, 2);
    const double poll_time = 0.2; // sec
    static double time2upd = 0.;
    time2upd -= delta;
    if (time2upd > 0.)
    {
        return;
    }
    char buf[4096];
    MYSQL_RES *pRes;
    MYSQL_ROW row;
    int_point ant_cur = {0,0};
    snprintf(buf,4096,"SELECT name,val FROM adrive_params WHERE name='Mode' \
OR name='cur_Az' OR name='cur_Za' OR name='new_Az' OR name='new_Za'");
    if (mysql_query(&mysql,buf))
    {
        snprintf(buf,4096,"[%s:%d] Error: can't send query to server:\n\t",
                 _FILE_,__LINE__);
        std::string err(buf);
        err+=mysql_error(&mysql);
        throw std::runtime_error(err);
    }
    pRes = mysql_use_result(&mysql);
    bool changeNewCoord = false;
    while ((row = mysql_fetch_row(pRes)))
    {
        if (!strcmp(row[0], "Mode"))
        {
            if (atoi(row[1]) != 2)
            {
                gotoSet = false;
                time2upd = 0.;
                break;
            }
        }
        if (!strcmp(row[0], "cur_Az"))
        {
            ant_cur.first = atoi(row[1]);
            continue;
        }
        if (!strcmp(row[0], "cur_Za"))
        {
            ant_cur.second = atoi(row[1]);
            continue;
        }
        if (!strcmp(row[0], "new_Az"))
        {
            int tmp = atoi(row[1]);
            if (tmp != new_az)
            {
                new_az = tmp;
                changeNewCoord = true;
            }
            continue;
        }
        if (!strcmp(row[0], "new_Za"))
        {
            int tmp = atoi(row[1]);
            if (tmp != new_za)
            {
                new_za = tmp;
                changeNewCoord = true;
            }
            continue;
        }
    }
    mysql_free_result(pRes);
    if (changeNewCoord)
    {
        qDebug()<<"changeNewCoord true";
        int_point ant = {new_az, new_za};
        double_point rls = antenna2radar(ant);
        gotoPoint = Vec3d(-cos(rls.first), sin(rls.first), tan(rls.second));
        gotoPoint.normalize();
    }
    double diff;
    diff = pow(new_az - ant_cur.first, 2) + pow(new_za - ant_cur.second, 2);
    qDebug()<<"diff:"<<diff;
//     if (diff <= delta_sq)
//     {
//         setGotoPoint(false);
//         time2upd = 0.;
//     }
    time2upd = poll_time;
}

void SatTrajMgr::drawAdjInfo(StelCore *core, StelPainter& painter)
{
    const StelProjector::StelProjectorParams spp =
                                          core->getCurrentStelProjectorParams();
    painter.setColor(textColor.redF(), textColor.greenF(), textColor.blueF(),
                     adjInfoFader.getInterstate());
    painter.setFont(messageFont);
    painter.drawText(spp.viewportXywh[2]-gWidth,
                     spp.viewportXywh[3]-gHeight+2*lineSpacing,
        QString("     Azimuth adjustment (step):  %1  (%2) [ang sec]")
        .arg(azAdj).arg(azIncr)
    );
    painter.drawText(spp.viewportXywh[2]-gWidth,
                     spp.viewportXywh[3]-gHeight+1*lineSpacing,
        QString("Zenith angle adjustment (step):  %1  (%2) [ang sec]")
        .arg(zaAdj).arg(zaIncr)
    );
    painter.drawText(spp.viewportXywh[2]-gWidth, spp.viewportXywh[3]-gHeight,
        QString("             Goto point is set:  %1")
        .arg(gotoSet?"true":"false")
    );
}

void SatTrajMgr::handleMouseClicks(QMouseEvent *event)
{
    if (flagShowSatTraj && antennaSelected &&
        event->button() == Qt::LeftButton &&
        event->modifiers() == Qt::ControlModifier
    )
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            gotoXY = event->pos();
            setGotoPoint(true);
        }
        event->accept();
    }
    if (flagShowSatTraj &&
        event->button() == Qt::RightButton &&
        event->modifiers() == Qt::ControlModifier
    )
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            setGotoPoint(false);
        }
        event->accept();
    }
}

QList<StelObjectP> SatTrajMgr::searchAround(const Vec3d& av, double limitFov,
                                            const StelCore* core) const
{
  QList<StelObjectP> result;
  if (!flagShowSatTraj || !dbIsUp)
    return result;

  Vec3d v(av);
  v.normalize();
  double cosLimFov = cos(limitFov * M_PI/180.);
  Vec3d pos;

  foreach(const GenObjP& traj, objects)
  {
    if (traj && traj->isInit())
    {
      pos = traj->getJ2000EquatorialPos(core);
      pos.normalize();
      if (pos[0]*v[0] + pos[1]*v[1] + pos[2]*v[2]>=cosLimFov)
      {
        result.append(qSharedPointerCast<StelObject>(traj));
      }
    }
  }
  return result;
}

StelObjectP SatTrajMgr::searchByNameI18n(const QString& nameI18n) const
{
  if (!flagShowSatTraj || !dbIsUp)
    return NULL;

  QString objw = nameI18n.toUpper();

  foreach(const GenObjP& traj, objects)
  {
    if (traj && traj->isInit())
    {
      if (traj->getNameI18n().toUpper() == nameI18n)
        return qSharedPointerCast<StelObject>(traj);
    }
  }
  return NULL;
}

StelObjectP SatTrajMgr::searchByName(const QString& englishName) const
{
  if (!flagShowSatTraj || !dbIsUp)
    return NULL;

  QString objw = englishName.toUpper();
  foreach(const GenObjP& traj, objects)
  {
    if (traj && traj->isInit())
    {
      if (traj->getEnglishName().toUpper() == englishName)
        return qSharedPointerCast<StelObject>(traj);
    }
  }
  return NULL;
}

QStringList SatTrajMgr::listMatchingObjectsI18n(const QString& objPrefix,
                                                int maxNbItem) const
{
  QStringList result;
  if (!flagShowSatTraj || !dbIsUp)
    return result;
  if (maxNbItem==0) return result;

  QString objw = objPrefix.toUpper();

  foreach(const GenObjP& traj, objects)
  {
    if (traj && traj->isInit())
    {
      if (traj->getNameI18n().toUpper().left(objw.length()) == objw)
      {
        result << traj->getNameI18n().toUpper();
      }
    }
  }
  result.sort();
  if (result.size()>maxNbItem)
      result.erase(result.begin()+maxNbItem, result.end());
  return result;
}

bool SatTrajMgr::initMysql(MYSQL &mysql)
{
    if (!mysql_init(&mysql))
    {
        char buf[1024];
        snprintf(buf, 1024, "[%s:%d] Error: can't init MYSQL struct",
                 _FILE_, __LINE__);
        std::string err(buf);
        qWarning() << err.c_str();
        return false;
    }
    const bool IsReconnect = true;
    if (mysql_options(&mysql,MYSQL_OPT_RECONNECT,(const char*)&IsReconnect))
    {
        char buf[1024];
        snprintf(buf, 1024,
                 "[%s:%d] Error: can't set option MYSQL_OPT_RECONNECT:\n\t",
                 _FILE_, __LINE__);
        std::string err(buf);
        err+=mysql_error(&mysql);
        mysql_close(&mysql);
        qWarning() << err.c_str();
        return false;
    }
    if (!mysql_real_connect(&mysql,host.c_str(),user.c_str(),pass.c_str(),
        database.c_str(),port,NULL,0))
    {
        char buf[1024];
        snprintf(buf,1024,"[%s:%d] Error: can't connect to server %s:%d:\n\t",
                 _FILE_,__LINE__,host.c_str(),port);
        std::string err(buf);
        err+=mysql_error(&mysql);
        mysql_close(&mysql);
        qWarning() << err.c_str();
        return false;
    }
    return true;
}

void SatTrajMgr::changeSyncPeriod(double newPer)
{
    if (newPer == syncPeriod)
        return;

    syncPeriod = newPer;
    if (ntpSync)
    {
        delete ntpSync;
        ntpSync = new NtpSync(this, (int)(syncPeriod*1e3));
    }
}

void* SatTrajMgr::dbRoutine()
{
    static int trajNum = 0; // traj number to update
    dbSecondIsUp = false;
    pthread_cleanup_push(callCleanup, this);

    while (true)
    {
        if (!dbSecondIsUp)
        {
            dbSecondIsUp = initMysql(mysqlSecond);
        }
//         qDebug() << "dbRoutine";
        if (!pthread_mutex_trylock(&dbLock))
        {
            pthread_cond_wait(&dbCv, &dbLock);
            // do DB update
            if (dbSecondIsUp)
            {
                GenObjP obj;
                switch (trajNum)
                {
                    case 0:
                        obj = pMeasTraj;
                        break;
                    case 1:
                        obj = pTdTraj;
                        break;
                    case 2:
                        obj = pRefTraj;
                        break;
                    case 3:
                        obj = pEstTraj;
                        break;
                    case 4:
                        obj = pDebugTraj;
                        break;
                    case 5:
                        obj = pAntTraj;
                        break;
                }
                if (obj && obj->isInit())
                {
                    obj->baseUpdate();
                }
                ++trajNum %= 6;
            }
            if (enableGoodSamples && !trajNum)
            {
                updGoodSamples();
            }
            pthread_mutex_unlock(&dbLock);
        }
        pthread_testcancel();
    }
    pthread_cleanup_pop(1);
    return NULL;
}

void SatTrajMgr::dbRoutCleanup()
{
    if (dbSecondIsUp)
    {
        mysql_close(&mysqlSecond);
        dbSecondIsUp = false;
    }
}

void SatTrajMgr::updGoodSamples(void )
{
    char query[256];
    snprintf(query, 256,
             "SELECT Time,pAz,pUm,Dist,ID FROM InterCnTrack WHERE "
             "ID>%d AND (ID%%200)=0 ORDER BY ID ASC", lastGoodMeasID);
    MYSQL_RES *pRes;
    MYSQL_ROW row;
    double curAz, curEl, dist;
    Vec3d pos;
    xNtpTime time;
    GenObject::getStelTimeNTP(GoodSample::drawTime);
    if (mysql_query(&mysqlSecond, query))
    {
        char buf[1024];
        snprintf(buf, 1024, "[%s:%d] Error: can't send query to server:\n",
                 _FILE_, __LINE__);
        std::string err(buf);
        err += mysql_error(&mysqlSecond);
        throw std::runtime_error(err);
    }
    pRes = mysql_use_result(&mysqlSecond);
    while ((row = mysql_fetch_row(pRes)))
    {
        lastGoodMeasID = atoi(row[4]);
        if (changeDecimal)
        {
            for (register uint i = 0; i < sizeof(row[1]); ++i)
                if (row[1][i] == '.')
                    row[1][i] = ',';
            for (register uint i = 0; i < sizeof(row[2]); ++i)
                if (row[2][i] == '.')
                    row[2][i] = ',';
        }
        curAz = atof(row[1]);
        curEl = atof(row[2]);
        // фикс для расчёта тангенса около пи/2
        if (curEl >= M_PI_2 - 1e-9)
        {
            curEl -= 1e-9;
        }
        else if (curEl <= -M_PI_2 + 1e-9)
        {
            curEl += 1e-9;
        }
        time = (u64)strtoull(row[0], NULL, 10);
        pos = Vec3d(-cos(curAz), sin(curAz), tan(curEl));
        pos.normalize();
        dist = atof(row[3]);
        DataPoint ref = pTdTraj->findByTime(time);
        double daz = 9999., del = 9999.;
        if (ref.time != (uint64_t)0)
        {
            // Перевод в СК РЛС из внутреннего формата stellarium
            double ref_az, ref_el;
            double az_inv_s1, az_inv_s2, az_inv_c1, az_inv_c2;
            az_inv_s1 = asin(ref.vec[1]/sqrt(1.-ref.vec[2]*ref.vec[2]));
            if (az_inv_s1 >= 0.)
                az_inv_s2 = M_PI - az_inv_s1;
            else
                az_inv_s2 = -M_PI - az_inv_s1;
            az_inv_c1 = acos(-ref.vec[0]/sqrt(1.-ref.vec[2]*ref.vec[2]));
            az_inv_c2 = -az_inv_c1;
            double diff, min_diff;
            int min_i = 0;
            // i == 0
            min_diff = diff = fabs(az_inv_s1 - az_inv_c1);
            // i == 1
            if ((diff = fabs(az_inv_s1 - az_inv_c2)) < min_diff)
            {
                min_diff = diff;
                min_i = 1;
            }
            // i == 2
            if ((diff = fabs(az_inv_s2 - az_inv_c1)) < min_diff)
            {
                min_diff = diff;
                min_i = 2;
            }
            // i == 3
            if ((diff = fabs(az_inv_s2 - az_inv_c2)) < min_diff)
            {
                min_diff = diff;
                min_i = 3;
            }
            switch (min_i)
            {
                case 0: case 1:
                    ref_az = az_inv_s1;
                    break;
                case 2: case 3:
                    ref_az = az_inv_s2;
                    break;
            }
            ref_el = atan(ref.vec[2]/sqrt(1.-ref.vec[2]*ref.vec[2]));

            daz = (curAz - ref_az)*180.*60./M_PI;
            del = (curEl - ref_el)*180.*60./M_PI;
        }
        objects.append(GenObjP(new GoodSample("Good sample", pDebugColor,
                                              time, pos, dist, daz, del)));
    }
    mysql_free_result(pRes);
}

void SatTrajMgr::setEnableShm(bool b)
{
    if (enableShm != b)
    {
        enableShm = b;
        emit changeEnableShm(b);
    }
}

/* -------------------------------------------------------------------------- */

NtpSync::NtpSync(SatTrajMgr *p, int to)
    : daemonIsUp(false)
    , mgr(p)
    , resyncTo(to)
{
//     qDebug() << "NtpSync()";
    ntptime_shm_init(&ntpCont);
    resyncTimer = new QTimer(this);
    connect(resyncTimer, SIGNAL(timeout()), this, SLOT(resync()));
    resyncTimer->start(resyncTo);
}

NtpSync::~NtpSync()
{
//     qDebug() << "~NtpSync()";
    ntptime_shm_deinit(&ntpCont);
    delete resyncTimer;
    resyncTimer = NULL;
    mgr = NULL;
}

void NtpSync::draw(StelPainter& painter)
{
    const StelProjectorP prj = painter.getProjector();
    QColor c = mgr->getTextColor();
    painter.setColor(c.redF(), c.greenF(), c.blueF());
    painter.setFont(mgr->getFont());
    if (!mgr->hasDB())
    {
        painter.drawText(prj->getViewportWidth()-mgr->gWidth, mgr->gHeight,
                        QString("Can't connect to DB"));
        return;
    }
    if (daemonIsUp)
    {
        painter.drawText(prj->getViewportWidth()-mgr->gWidth, mgr->gHeight,
                         QString("Ntptimed is working, sync period %1 sec")
                         .arg(resyncTo/1e3,0,'f'));
    }
    else
    {
        painter.drawText(prj->getViewportWidth()-mgr->gWidth, mgr->gHeight,
                         QString("Ntptimed is NOT working, sync period %1 sec")
                         .arg(resyncTo/1e3,0,'f'));
    }
}

void NtpSync::resync()
{
//     qDebug() << "NtpSync::resync()";
    if (!mgr->hasDB())
        return;

    // Check whether Ntptimed daemon is running
    char buf[1024];
    MYSQL_RES *pRes;
    MYSQL_ROW row;
    snprintf(buf, 1024, "SELECT val FROM ntptimed_params WHERE name='Enabled'");
    if (mysql_query(&mgr->mysql, buf))
    {
        snprintf(buf, 1024, "[%s:%d] Error: can't send query to server:\n",
                 _FILE_, __LINE__);
        std::string err(buf);
        err += mysql_error(&mgr->mysql);
        throw std::runtime_error(err);
    }
    pRes = mysql_use_result(&mgr->mysql);
    row = mysql_fetch_row(pRes);
    int res = atoi(row[0]);
    mysql_free_result(pRes);
    switch (res)
    {
        case 0:
//             qDebug() << "daemon is down";
            daemonIsUp = false;
            break;
        case 1:
//             qDebug() << "daemon is up";
            daemonIsUp = true;
            break;
        default:
//             qWarning() << "NtpSync::resync. Unknown value received";
            daemonIsUp = false;
    }
    if (!daemonIsUp)
        return;

    // Get NTP system time and set stellarium model time
    StelCore *core = StelApp::getInstance().getCore();
    double curNtpTime = ntptime_shm_getlastd(&ntpCont);
    core->setJDay((curNtpTime - (double)NTP_UNIX_DELTA)/86400. + 2440587.5);
    core->setRealTimeSpeed();
//     qDebug() << "NTP time is set";
}

/* -------------------------------------------------------------------------- */

SecProcInfo::SecProcInfo(SatTrajMgr* p, double to)
  : updPeriod_(to)
  , timeToUpd_(0.)
  , mgr_(p)
  , state_("N/A")
  , target_("N/A")
{
//     qDebug()<<"SecProcInfo()";
}

SecProcInfo::~SecProcInfo()
{
//     qDebug()<<"~SecProcInfo()";
}

void SecProcInfo::update(double deltaTime)
{
//     qDebug()<<"SecProcInfo::update";
    timeToUpd_ -= deltaTime;
    if (timeToUpd_ > 0.)
        return;
    timeToUpd_ = updPeriod_;
    if (!mgr_->hasDB())
        return;
//     qDebug()<<"SecProcInfo::update mysql query";
    char buf[1024] = "SELECT val FROM sec_processing_params WHERE \
name='Status' OR name='Target' ORDER BY ID ASC";
    MYSQL_RES *pRes;
    MYSQL_ROW row;
    if (mysql_query(&mgr_->mysql, buf))
    {
        snprintf(buf, 1024, "[%s:%d] Error: can't send query to server:\n",
                 _FILE_, __LINE__);
        std::string err(buf);
        err += mysql_error(&mgr_->mysql);
        throw std::runtime_error(err);
    }
    pRes = mysql_use_result(&mgr_->mysql);
    if ((row = mysql_fetch_row(pRes)))
    {
        state_.assign(row[0]);
    }
    else
    {
        state_.assign("MYSQL query error");
    }
    if ((row = mysql_fetch_row(pRes)))
    {
        target_.assign(row[0]);
    }
    else
    {
        target_.assign("MYSQL query error");
    }
    mysql_free_result(pRes);
}

void SecProcInfo::draw(StelPainter& painter)
{
    const StelProjectorP prj = painter.getProjector();
    QColor c = mgr_->getTextColor();
    painter.setColor(c.redF(), c.greenF(), c.blueF());
    painter.setFont(mgr_->getFont());
    if (!mgr_->hasDB())
    {
        painter.drawText(prj->getViewportWidth()-mgr_->gWidth,
                         mgr_->gHeight - mgr_->getLineSpacing(),
                         QString("Can't connect to DB"));
    }
    else
    {
        painter.drawText(prj->getViewportWidth()-mgr_->gWidth,
                         mgr_->gHeight - mgr_->getLineSpacing(),
                         QString("Sec_processing is %1, target: %2")
                         .arg(state_.c_str()).arg(target_.c_str()));
    }
}
