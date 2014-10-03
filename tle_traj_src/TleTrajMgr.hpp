#ifndef _TLETRAJMGR_HPP_
#define _TLETRAJMGR_HPP_

#include "StelObjectModule.hpp"
#include "StelLocation.hpp"
#include "StelTextureTypes.hpp"
#include "TleTraj.hpp"

#include <QtGui/QColor>
#include <QtGui/QStandardItemModel>

#include <sat_predict/sat_predict.h>

class Planet;
class StelButton;
class QPixmap;
class TleTrajDialog;

typedef QSharedPointer<TleTraj> TleTrajP;
typedef QSharedPointer<QStandardItem> StdItemP;

class TleFile : public QStandardItem
{
public:
    explicit TleFile(const QString &_file);
    ~TleFile();
    virtual int type(void) const
    {
        return QStandardItem::UserType;
    }

    struct tle_obj
    {
        void setVisible(bool state)
        {
            p->isVisible = state;
            if (state && firstTimeVis)
            {
                color = genNewColor();
                firstTimeVis = false;
            }
        }

        tle_t tle;
        TleTrajP p;
        StdItemP item;
        QColor color;
        bool firstTimeVis;  // true if has never been visible, false after isVisible=true for first time
    };
    QString file;
    QStringList file_contents;
    QList<struct tle_obj*> tles;

private:
    static QColor genNewColor(void);
    static QColor lastGenColor;
};

/*! \class TleTrajMgr
 *  \brief This plugin shows satellite and other trajectories.
 *
 *  Using sat_predict API builds the trajectory of some object from TLE file
 *  and plots it point by point.
 */
class TleTrajMgr : public StelObjectModule
{
    Q_OBJECT
public:
    TleTrajMgr();
    virtual ~TleTrajMgr();

    ///////////////////////////////////////////////////////////////////////////
    // Methods defined in the StelModule class
    virtual void init();
    virtual void deinit();
    virtual void update(double deltaTime);
    virtual void draw(StelCore* core);
    //! Determine which "layer" the plugin's drawing will happen on.
    virtual double getCallOrder(StelModuleActionName actionName) const;

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
            int maxNbItem = 5) const;

    /*! Implment this to tell the main Stellarium GUI that there is a GUI
     *  element to configure this plugin.
     */
    virtual bool configureGui(bool show = true);

    void setTimeWindow(uint newWindow);
    uint getTimeWindow(void) const
    {
        return TleTraj::timeWindow;
    }
    void setSegmentsNum(uint newNum);
    uint getSegmentsNum(void) const
    {
        return TleTraj::orbitLineSegments;
    }

public slots:
    void enableTleTrajMgr(bool b);

private slots:
    void observerLocationChanged(StelLocation loc);

private:
    QList<TleFile*> *tleFiles;
    bool flagShowTleTraj;
    QPixmap *pxmapGlow;
    QPixmap *pxmapOnIcon;
    QPixmap *pxmapOffIcon;
    StelButton *toolbarButton;
    StelTextureSP texPointer;
    QSharedPointer<Planet> earth;
    // GUI
    TleTrajDialog *configDialog;

    //! Restore default settings.
    void restoreDefaultConfigIni(void) const;
    //! Read settings from config.
    void readSettingsFromConfig(void);
    void saveConfigOnExit(void);
    void drawPointer(StelCore *core, StelPainter &painter) const;
    void recalculateOrbitLines(void);
};


#include "fixx11h.h"
#include <QtCore/QObject>
#include "StelPluginInterface.hpp"

/*! \class TleTrajMgrStelPluginInterface
 *  \brief This class is used by Qt to manage a plug-in interface.
 */
class TleTrajMgrStelPluginInterface : public QObject, public StelPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(StelPluginInterface)
public:
    virtual StelModule* getStelModule() const;
    virtual StelPluginInfo getPluginInfo() const;
};

#endif /*_TLETRAJMGR_HPP_*/
