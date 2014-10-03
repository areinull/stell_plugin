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
#include "SolarSystem.hpp"
#include "TleTrajMgr.hpp"
#include "TleTrajDialog.hpp"

#include <QtOpenGL/QtOpenGL>

StelModule* TleTrajMgrStelPluginInterface::getStelModule() const
{
    return new TleTrajMgr();
}

StelPluginInfo TleTrajMgrStelPluginInterface::getPluginInfo() const
{
    // Allow to load the resources when used as a static plugin
    Q_INIT_RESOURCE(TleTrajMgr);

    StelPluginInfo info;
    info.id = "TleTrajMgr";
    info.displayedName = "TLE Trajectory";
    info.authors = "";
    info.contact = "";
    info.description = "Provides TLE trajectories graphics";
    return info;
}

Q_EXPORT_PLUGIN2(TleTrajMgr, TleTrajMgrStelPluginInterface)


TleTrajMgr::TleTrajMgr():
        tleFiles(NULL), flagShowTleTraj(false), pxmapGlow(NULL),
        pxmapOnIcon(NULL), pxmapOffIcon(NULL), toolbarButton(NULL)
{
    tleFiles = new QList<TleFile*>;
    setObjectName("TleTrajMgr");
    configDialog = new TleTrajDialog(tleFiles);
}

TleTrajMgr::~TleTrajMgr()
{
    delete tleFiles;
    tleFiles = NULL;
    delete configDialog;
    configDialog = NULL;
}

double TleTrajMgr::getCallOrder(StelModuleActionName actionName) const
{
    if (actionName == StelModule::ActionDraw)
        return StelApp::getInstance().getModuleMgr().getModule("SolarSystem")->getCallOrder(actionName) + 1.;
    return 0;
}

void TleTrajMgr::init()
{
    QSettings *conf = StelApp::getInstance().getSettings();
    try
    {
        // If no settings in the main config file, create with defaults
        if (!conf->childGroups().contains("TleTrajMgr"))
        {
            restoreDefaultConfigIni();
        }

        // populate settings from main config file
        readSettingsFromConfig();

        // Load and find resources used in the plugin
        TleTraj::hintTexture = StelApp::getInstance().getTextureManager().createTexture(":/stell_plug/sat_hint_2.png");
        texPointer = StelApp::getInstance().getTextureManager().createTexture("textures/pointeur5.png");

        // add gui action
        QString groupName("TLE trajectories Key Bindings");
        StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
        gui->addGuiActions("actionShow_TleTrajMgr_ConfigDialog", "TLE Trajectories Config Dialog", "Alt+J", groupName);
        gui->addGuiActions("actionShow_TleTrajMgr", "TLE Trajectories", "Ctrl+J", groupName);
        gui->getGuiActions("actionShow_TleTrajMgr")->setChecked(flagShowTleTraj);
        connect(gui->getGuiActions("actionShow_TleTrajMgr_ConfigDialog"), SIGNAL(toggled(bool)), configDialog, SLOT(setVisible(bool)));
        connect(gui->getGuiActions("actionShow_TleTrajMgr"), SIGNAL(toggled(bool)), this, SLOT(enableTleTrajMgr(bool)));

        // Gui toolbar button
        pxmapGlow = new QPixmap(":/stell_plug/glow32x32.png");
        pxmapOnIcon = new QPixmap(":/stell_plug/bt_tletraj_on.png");
        pxmapOffIcon = new QPixmap(":/stell_plug/bt_tletraj_off.png");
        toolbarButton = new StelButton(NULL, *pxmapOnIcon, *pxmapOffIcon, *pxmapGlow, gui->getGuiActions("actionShow_TleTrajMgr"));
        gui->getButtonBar()->addButton(toolbarButton, "065-pluginsGroup");
    }
    catch (std::runtime_error &e)
    {
        qWarning() << "TleTrajMgr::init error: " << e.what();
        return;
    }

    earth = GETSTELMODULE(SolarSystem)->getEarth();
    GETSTELMODULE(StelObjectMgr)->registerStelObjectMgr(this);

    // Handle changes to the observer location:
    connect(StelApp::getInstance().getCore(), SIGNAL(locationChanged(StelLocation)), this, SLOT(observerLocationChanged(StelLocation)));
    observerLocationChanged(StelApp::getInstance().getCore()->getCurrentLocation());
}

void TleTrajMgr::observerLocationChanged(StelLocation loc)
{
    TleTraj::location = loc;
    recalculateOrbitLines();
}

void TleTrajMgr::recalculateOrbitLines(void)
{
    foreach(const TleFile *tle_file, *tleFiles)
    {
        foreach(const struct TleFile::tle_obj *tleobj, tle_file->tles)
        {
            if (!tleobj->p.isNull() && tleobj->p->isInitialized && tleobj->p->isVisible)
                tleobj->p->recalculateOrbitLines();
        }
    }
}

bool TleTrajMgr::configureGui(bool show)
{
    if (show)
    {
        StelGui *gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
        gui->getGuiActions("actionShow_TleTrajMgr_ConfigDialog")->setChecked(true);
    }
    return true;
}

void TleTrajMgr::restoreDefaultConfigIni() const
{
    QSettings *conf = StelApp::getInstance().getSettings();
    conf->beginGroup("TleTrajMgr");

    // delete all existing settings...
    conf->remove("");

    conf->setValue("timeWindow", 1200);
    conf->setValue("segmentsNum", 90);

    conf->endGroup();
}

void TleTrajMgr::readSettingsFromConfig()
{
    QSettings *conf = StelApp::getInstance().getSettings();
    conf->beginGroup("TleTrajMgr");

    TleTraj::timeWindow = conf->value("timeWindow").toUInt();
    TleTraj::orbitLineSegments = conf->value("segmentsNum").toUInt();
    QStringList keys = conf->allKeys();
    foreach(const QString &key, keys)
    {
        if (key.startsWith("TLEfile") && StelFileMgr::exists(conf->value(key).toString()))
        {
            TleFile *tmp = NULL;
            try
            {
                tmp = new TleFile(conf->value(key).toString());
            }
            catch (QString &e)
            {
                if (e == QString("TleFile"))
                {
                    qDebug() << "Nonexistent TLE file: " << conf->value(key).toString();
                    delete tmp;
                }
                else
                    throw;
            }
            tleFiles->append(tmp);
        }
    }

    conf->endGroup();
}

void TleTrajMgr::saveConfigOnExit(void)
{
    QSettings *conf = StelApp::getInstance().getSettings();
    conf->beginGroup("TleTrajMgr");

    conf->setValue("timeWindow", TleTraj::timeWindow);
    conf->setValue("segmentsNum", TleTraj::orbitLineSegments);
    QStringList keys = conf->allKeys();
    foreach(const QString &key, keys)
    {
        if (key.startsWith("TLEfile"))
        {
            conf->remove(key);
        }
    }
    for (int i = 0; i < tleFiles->size(); ++i)
    {
        conf->setValue(QString("TLEfile%1").arg(i), tleFiles->at(i)->file);
    }

    conf->endGroup();
}

void TleTrajMgr::deinit(void)
{
    saveConfigOnExit();
    TleTraj::hintTexture.clear();
    texPointer.clear();
    foreach(TleFile *tle_file, *tleFiles)
    {
        delete tle_file;
    }
    tleFiles->clear();
}

void TleTrajMgr::update(double /*deltaTime*/)
{
    if (!flagShowTleTraj || StelApp::getInstance().getCore()->getCurrentLocation().planetName != earth->getEnglishName())
        return;

    foreach(const TleFile *tle_file, *tleFiles)
    {
        foreach(const struct TleFile::tle_obj *tleobj, tle_file->tles)
        {
            if (!tleobj->p.isNull() && tleobj->p->isInitialized && tleobj->p->isVisible)
                tleobj->p->update();
        }
    }
}

void TleTrajMgr::enableTleTrajMgr(bool b)
{
    qDebug() << "TleTrajMgr::enableTleTrajMgr" << b;
    flagShowTleTraj = b;
}

void TleTrajMgr::draw(StelCore* core)
{
    if (!flagShowTleTraj || core->getCurrentLocation().planetName != earth->getEnglishName())
    {
        if (GETSTELMODULE(StelObjectMgr)->getWasSelected())
        {
            const QList<StelObjectP> newSelected = GETSTELMODULE(StelObjectMgr)->getSelectedObject("TleTraj");
            if (!newSelected.isEmpty())
                GETSTELMODULE(StelObjectMgr)->unSelect();
        }
        return;
    }
    StelProjectorP prj = core->getProjection(StelCore::FrameAltAz);
    StelPainter painter(prj);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    TleTraj::viewportHalfspace = prj->getBoundingCap();
    foreach(const TleFile *tle_file, *tleFiles)
    {
        foreach(const struct TleFile::tle_obj *tleobj, tle_file->tles)
        {
            if (!tleobj->p.isNull() && tleobj->p->isInitialized && tleobj->p->isVisible)
                tleobj->p->draw(core, painter);
        }
    }

    // Draw pointer
    if (GETSTELMODULE(StelObjectMgr)->getWasSelected())
        drawPointer(core, painter);
}

void TleTrajMgr::drawPointer(StelCore *core, StelPainter &painter) const
{
    const QList<StelObjectP> newSelected = GETSTELMODULE(StelObjectMgr)->getSelectedObject("TleTraj");
    if (!newSelected.isEmpty())
    {
        const StelObjectP obj = newSelected[0];
        const StelProjectorP prj = core->getProjection(StelCore::FrameJ2000);
        if (dynamic_cast<TleTraj*>(obj.data())->isVisible)
        {
            Vec3d pos = obj->getJ2000EquatorialPos(core);
            Vec3d screenpos;

            // Compute 2D pos and return if outside screen
            if (!prj->project(pos, screenpos))
                return;
            glColor3f(0.4f, 0.5f, 0.8f);
            texPointer->bind();

            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Normal transparency mode

            // Size on screen
            float size = obj->getAngularSize(core) * M_PI / 180.*prj->getPixelPerRadAtCenter();
            size += 12.f + 5.f * std::sin(2.f * StelApp::getInstance().getTotalRunTime());
            painter.drawSprite2dMode(screenpos[0] - size / 2, screenpos[1] - size / 2, 20, 90);
            painter.drawSprite2dMode(screenpos[0] - size / 2, screenpos[1] + size / 2, 20, 0);
            painter.drawSprite2dMode(screenpos[0] + size / 2, screenpos[1] + size / 2, 20, -90);
            painter.drawSprite2dMode(screenpos[0] + size / 2, screenpos[1] - size / 2, 20, -180);

            glDisable(GL_TEXTURE_2D);
        }
        else
        {
            GETSTELMODULE(StelObjectMgr)->unSelect();
        }
    }
}

QList<StelObjectP> TleTrajMgr::searchAround(const Vec3d& av, double limitFov, const StelCore* core) const
{
    QList<StelObjectP> result;
    if (!flagShowTleTraj || core->getCurrentLocation().planetName != earth->getEnglishName())
        return result;

    Vec3d v(av);
    v.normalize();
    double cosLimFov = cos(limitFov * M_PI / 180.);
    Vec3d pos;

    foreach(const TleFile *tle_file, *tleFiles)
    {
        foreach(const struct TleFile::tle_obj *tleobj, tle_file->tles)
        {
            if (!tleobj->p.isNull() && tleobj->p->isInitialized && tleobj->p->isVisible)
            {
                pos = tleobj->p->getJ2000EquatorialPos(core);
                pos.normalize();
                if (pos[0]*v[0] + pos[1]*v[1] + pos[2]*v[2] >= cosLimFov)
                {
                    result.append(qSharedPointerCast<StelObject>(tleobj->p));
                }
            }
        }
    }
    return result;
}

StelObjectP TleTrajMgr::searchByNameI18n(const QString& nameI18n) const
{
    if (!flagShowTleTraj || StelApp::getInstance().getCore()->getCurrentLocation().planetName != earth->getEnglishName())
        return NULL;

    QString objw = nameI18n.toUpper();

    foreach(const TleFile *tle_file, *tleFiles)
    {
        foreach(const struct TleFile::tle_obj *tleobj, tle_file->tles)
        {
            if (!tleobj->p.isNull() && tleobj->p->isInitialized && tleobj->p->isVisible)
                if (tleobj->p->getNameI18n().toUpper() == nameI18n)
                    return qSharedPointerCast<StelObject>(tleobj->p);
        }
    }
    return NULL;
}

StelObjectP TleTrajMgr::searchByName(const QString& englishName) const
{
    if (!flagShowTleTraj || StelApp::getInstance().getCore()->getCurrentLocation().planetName != earth->getEnglishName())
        return NULL;

    QString objw = englishName.toUpper();

    foreach(const TleFile *tle_file, *tleFiles)
    {
        foreach(const struct TleFile::tle_obj *tleobj, tle_file->tles)
        {
            if (!tleobj->p.isNull() && tleobj->p->isInitialized && tleobj->p->isVisible)
                if (tleobj->p->getEnglishName().toUpper() == englishName)
                    return qSharedPointerCast<StelObject>(tleobj->p);
        }
    }
    return NULL;
}

QStringList TleTrajMgr::listMatchingObjectsI18n(const QString& objPrefix,
        int maxNbItem) const
{
    QStringList result;
    if (!flagShowTleTraj || StelApp::getInstance().getCore()->getCurrentLocation().planetName != earth->getEnglishName())
        return result;
    if (maxNbItem == 0)
        return result;

    QString objw = objPrefix.toUpper();

    foreach(const TleFile *tle_file, *tleFiles)
    {
        foreach(const struct TleFile::tle_obj *tleobj, tle_file->tles)
        {
            if (!tleobj->p.isNull() && tleobj->p->isInitialized && tleobj->p->isVisible)
                if (tleobj->p->getNameI18n().toUpper().left(objw.length()) == objw)
                    result << tleobj->p->getNameI18n().toUpper();
        }
    }
    result.sort();
    if (result.size() > maxNbItem)
        result.erase(result.begin() + maxNbItem, result.end());
    return result;
}

void TleTrajMgr::setTimeWindow(uint newWindow)
{
    TleTraj::timeWindow = newWindow;
    recalculateOrbitLines();
}

void TleTrajMgr::setSegmentsNum(uint newNum)
{
    TleTraj::orbitLineSegments = newNum;
    recalculateOrbitLines();
}

/* -------------------------------------------------------------------------- */
QColor TleFile::lastGenColor = QColor(Qt::transparent);

TleFile::TleFile(const QString& _file): QStandardItem(_file), file(_file)
{
    if (!StelFileMgr::exists(_file))
      throw QString("TleFile");
    setCheckable(false);
    setEditable(false);
    tle_t *new_tle = NULL;
    int tle_num = 0;
    struct tle_obj *tmp_struct;

    new_tle = read_tle_txt(file.toStdString().c_str(), &tle_num);
    qDebug() << "read" << tle_num << "objects from file:" << file;
    if (tle_num <= 0)
    {
        free(new_tle);
        throw QString("TleFile");
    }
    for (int i = 0; i < tle_num; ++i)
    {
        tmp_struct = new struct tle_obj;
        memcpy(&tmp_struct->tle, &new_tle[i], sizeof(tmp_struct->tle));
        tmp_struct->color = QColor(Qt::darkGray);
        tmp_struct->firstTimeVis = true;
        tmp_struct->item = StdItemP(new QStandardItem(QVariant(i).toString()+" "+QString(tmp_struct->tle.sat_name)));
        tmp_struct->item->setCheckable(false);
        tmp_struct->item->setEditable(false);
        tles.append(tmp_struct);
    }
    free(new_tle);
    new_tle = NULL;

    foreach(struct tle_obj *it, tles)
    {
        it->p = TleTrajP(new TleTraj);
        if (!it->p.isNull())
            it->p->isInitialized = true;
        else
            continue;
        it->p->tle = &it->tle;
        it->p->orbitColor = &it->color;
        it->p->name = it->tle.sat_name;
    }

    QFile tle_file(file);
    if (!tle_file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw QString("TleFile");
    QTextStream in(&tle_file);
    while (!in.atEnd())
    {
        file_contents << in.readLine();
    }
}

TleFile::~TleFile()
{
//     qDebug()<<"~TleFile()";
    file_contents.clear();
    foreach(struct tle_obj *it, tles)
    {
        takeChild(it->item->row());
        delete it;
    }
    tles.clear();
//     qDebug()<<"end of ~TleFile()";
}

QColor TleFile::genNewColor(void)
{
    if (!lastGenColor.alpha())
    {
        lastGenColor = QColor(Qt::red);
        qDebug() << "New color generated " << lastGenColor;
        return lastGenColor;
    }
    else
    {
        lastGenColor.setHsv(lastGenColor.hue() + (360 - 36) / 4, lastGenColor.saturation(), lastGenColor.value());
        qDebug() << "New color generated " << lastGenColor;
        return lastGenColor;
    }
}
