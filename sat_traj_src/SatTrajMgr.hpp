#ifndef _SATTRAJMGR_HPP_
#define _SATTRAJMGR_HPP_

#include "StelObjectModule.hpp"
#include "GenObject.hpp"
#include "GenTraj.hpp" // FIXME удалить потом зависимости
#include "StelFader.hpp"

#include <QtGui/QFont>
#include <QtGui/QColor>
#include <QtCore/QPoint>
#include <pthread.h>

#include <ntptime/ntptime.h>

class StelButton;
class QSettings;
class QPixmap;
class QTimer;
class QMouseEvent;
class SatTrajDialog;

typedef QSharedPointer<GenObject> GenObjP;
typedef QSharedPointer<GenTraj> GenTrajP;
typedef QSharedPointer<QColor> QColorP;

class NtpSync : public QObject
{
    Q_OBJECT

    bool daemonIsUp;
    ntptime_shm_con ntpCont;
    SatTrajMgr *mgr;
    QTimer *resyncTimer;
    const int resyncTo; // in ms

public:
    explicit NtpSync(SatTrajMgr *p, int to);
    ~NtpSync();

    void draw(StelPainter& painter);

private slots:
    void resync();
};

class SecProcInfo
{
    const double updPeriod_;
    double timeToUpd_;
    SatTrajMgr *const mgr_;
    std::string state_;
    std::string target_;

public:
    explicit SecProcInfo(SatTrajMgr *p, double to);
    ~SecProcInfo();

    void draw(StelPainter &painter);
    void update(double deltaTime);
};

/*! \class SatTrajMgr
 *  \brief This plugin shows satellite and other trajectories.
 *
 *  Using data coming via MYSQL database the trajectory of some
 *  object is plotted point by point. It's intended to use altazimuthal
 *  incoming coordinates with known position of the observer.
 */
class SatTrajMgr : public StelObjectModule
{
  Q_OBJECT

  public:
    SatTrajMgr();
    virtual ~SatTrajMgr();

    ///////////////////////////////////////////////////////////////////////////
    // Methods defined in the StelModule class
    virtual void init();
    virtual void deinit();
    virtual void update(double deltaTime);
    virtual void draw(StelCore* core);
    virtual double getCallOrder(StelModuleActionName actionName) const;
    virtual void handleMouseClicks(QMouseEvent*);

    ///////////////////////////////////////////////////////////////////////////
    // Methods defined in StelObjectManager class
    //! Used to get a list of objects which are near to some position.
    virtual QList<StelObjectP> searchAround(const Vec3d& v, double limitFov,
                                            const StelCore* core) const;
    //! Return the matching sattraj object's pointer if exists or NULL.
    virtual StelObjectP searchByNameI18n(const QString& nameI18n) const;
    //! Return the matching sattraj if exists or NULL.
    virtual StelObjectP searchByName(const QString& name) const;
    /*! Find and return the list of at most maxNbItem objects auto-completing
     *  the passed object I18n name.
     */
    virtual QStringList listMatchingObjectsI18n(const QString& objPrefix,
                                                int maxNbItem=5) const;
    /*! Implment this to tell the main Stellarium GUi that there is a GUI
     *  element to configure this plugin.
     */
    virtual bool configureGui(bool show=true);

    const QFont& getFont(void) const {return messageFont;}
    int getLineSpacing(void) const {return lineSpacing;}
    bool hasDB(void) const {return dbIsUp;}
    QColor getMeasTrColor(void) const {return *pMeasColor;}
    QColor getTdTrColor(void) const {return *pTdColor;}
    QColor getRefTrColor(void) const {return *pRefColor;}
    QColor getEstTrColor(void) const {return *pEstColor;}
    QColor getDebTrColor(void) const {return *pDebugColor;}
    QColor getAntTrColor(void) const {return *pAntColor;}
    QColor getTextColor(void) const {return textColor;}
    double getDbUpdTime(void) const {return baseUpdTime;}
    void setDbUpdTime(double t) {baseUpdTime = t;}
    void setEnableShm(bool b);

    std::string database;
    std::string user;
    std::string pass;
    std::string host;
    unsigned int port;
    std::string tableName;
    uint timeWindow;
    double timeWindowSamples; // window for primary samples drawing
    MYSQL mysql;
    MYSQL mysqlSecond;  // used for trajectory update
    int azAdj, zaAdj;   // in ang sec
    int azIncr, zaIncr; // in ang sec
    double syncPeriod;  // in sec
    bool antExtr; // use antenna trajectory extrapolation
    const double antExtrK;
    double antExtrTime;
    bool changeDecimal;
    const int gWidth;
    const int gHeight;
    bool enableGoodSamples;
    bool enableShm;

  signals:
    void changeEnableShm(bool);

  public slots:
    void enableSatTrajMgr(bool b);
    void setAzIncr(int val);
    void setZaIncr(int val);
    void changeSyncState(bool b);
    void changeSyncPeriod(double newPer);
    void setMeasTrColor(QColor c) {*pMeasColor = c;}
    void setTdTrColor(QColor c) {*pTdColor = c;}
    void setRefTrColor(QColor c) {*pRefColor = c;}
    void setEstTrColor(QColor c) {*pEstColor = c;}
    void setDebTrColor(QColor c) {*pDebugColor = c;}
    void setAntTrColor(QColor c) {*pAntColor = c;}
    void setTextColor(QColor c) {textColor = c;}

  private slots:
    void clearMessage(void);
    void adriveAzIncr(void);
    void adriveAzDecr(void);
    void adriveZaIncr(void);
    void adriveZaDecr(void);
    void resetAdj(void);

  private:
    QVector<GenObjP> objects;
    GenTrajP pMeasTraj;
    QColorP  pMeasColor;
    GenTrajP pTdTraj;
    QColorP  pTdColor;
    GenTrajP pRefTraj;
    QColorP  pRefColor;
    GenTrajP pEstTraj;
    QColorP  pEstColor;
    GenTrajP pDebugTraj;
    QColorP  pDebugColor;
    GenTrajP pAntTraj;
    QColorP  pAntColor;
    QColor textColor;
    bool flagShowSatTraj;
    LinearFader messageFader;
    LinearFader adjInfoFader;
    QSettings *settings;
    QTimer *messageTimer;
    QFont messageFont;
    QPixmap *pxmapGlow;
    QPixmap *pxmapOnIcon, *pxmapOffIcon;
    QPixmap *resetOffIcon, *resetOnIcon;
    QPixmap *syncOffIcon, *syncOnIcon;
    StelButton *toolbarButton, *tbbReset, *tbbSync;
    StelTextureSP texPointer, arrowTex;
    NtpSync *ntpSync;
    SecProcInfo *secProcInfo;
    bool dbIsUp;
    bool antennaSelected;
    bool gotoSet;
    QPoint gotoXY;
    Vec3d gotoPoint; // in frame StelCore::FrameAltAz
    int new_az, new_za;
    double baseUpdTime; // in sec
    pthread_t dbThread; // поток считывания данных траекторий из БД
    // для сигнализации необходимости произвести обновление из БД
    pthread_mutex_t dbLock;
    // для сигнализации необходимости произвести обновление из БД
    pthread_cond_t dbCv;
    bool dbSecondIsUp;
    // номер последней полученной точки "хорошего" измерений из БД
    // FIXME сейчас хранятся все точки
    // FIXME для отладки в качестве хороших измерений выбирается каждое 200-е
    int lastGoodMeasID;
    // GUI
    SatTrajDialog *configDialog;
    int lineSpacing;

    //! initialize MYSQL connection
    bool initMysql(MYSQL &);
    //! Restore default settings.
    void restoreDefaultConfigIni(void);
    //! Read settings from config.
    void readSettingsFromConfig(void);
    void updAzAdj(void);
    void updZaAdj(void);
    void getAdj(void);
    void drawAdjInfo(StelCore *core, StelPainter& painter);
    void initTraj(void);
    void deinitTraj(void);
    //! Load and find resources used in the plugin
    void loadTex(void);
    void setGotoPoint(bool);
    void gotoFunc(double);
    void* dbRoutine(void);
    void dbRoutCleanup(void);
    void updGoodSamples(void);

    static void* callRoutine(void *arg) {return ((SatTrajMgr*)arg)->dbRoutine();}
    static void callCleanup(void *arg) {((SatTrajMgr*)arg)->dbRoutCleanup();}
};


#include "fixx11h.h"
#include <QtCore/QObject>
#include "StelPluginInterface.hpp"

/*! \class SatTrajMgrStelPluginInterface
 *  \brief This class is used by Qt to manage a plug-in interface.
 */
class SatTrajMgrStelPluginInterface : public QObject, public StelPluginInterface
{
  Q_OBJECT
  Q_INTERFACES(StelPluginInterface)
  public:
    virtual StelModule* getStelModule() const;
    virtual StelPluginInfo getPluginInfo() const;
};

#endif /*_SATTRAJMGR_HPP_*/
