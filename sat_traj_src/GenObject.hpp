#ifndef _GENOBJECT_HPP_
#define _GENOBJECT_HPP_

#include "StelObject.hpp"
#include "StelProjectorType.hpp"
#include <QColor>

class xNtpTime;
class StelCore;
class SatTrajMgr;
class StelPainter;
typedef QSharedPointer<QColor> QColorP;

class GenObject: public StelObject
{
    GenObject();
    GenObject(const GenObject&);
    const GenObject& operator=(const GenObject&);

public:
    GenObject(QString id, const QColorP &c)
        : name(id)
        , XYZ(0.,0.,0.)
        , curRange(0.)
        , initialized(false)
        , visible(false)
        , color(c)
        {}
    virtual ~GenObject() {}

    virtual QString getType(void) const =0;
    virtual QString getInfoString(const StelCore *core, const InfoStringGroup& flags) const =0;
    virtual void draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter)=0;
    virtual void baseUpdate(void)=0;

    virtual Vec3d getJ2000EquatorialPos(const StelCore *core) const;
    virtual QString getNameI18n() const {return name;}
    virtual QString getEnglishName() const {return name;}
    virtual float getSelectPriority(const StelCore * /*nav*/) const
        {return -10.f;}
    virtual double getAngularSize(const StelCore* /*core*/) const
        {return 0.00001;}
    bool isInit(void) const {return initialized;}
    bool isVisible(void) const {return visible;}
    Vec3d getAltAzPos(void) const {return XYZ;}
    const QColor& getColor(void) const {return *color;}
    void setColor(const QColor& c) {*color = c;}

    // Get Stellarium model time in NTP format.
    static void getStelTimeNTP(xNtpTime &time);

protected:
    const QString name;
    Vec3d XYZ;          // Current Alt/Az coords
    double curRange;    // Current range
    bool initialized;
    bool visible;
    QColorP color;

    // Prefetch time window size relative to draw window (>1)
    static const float wndRelSize;
    // Beam width [deg]
    static const float antennaCone;
    // Size of measurement point relative to beam width
    static const float measurementPerc;
};

#endif // _GENOBJECT_HPP_
