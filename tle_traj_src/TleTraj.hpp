#ifndef _TLETRAJ_HPP_
#define _TLETRAJ_HPP_

#include "VecMath.hpp"
#include "StelObject.hpp"
#include "StelLocation.hpp"
#include "StelTextureTypes.hpp"

#include <sat_predict/sat_predict.h>

class StelPainter;
class QColor;

/*! \class TleTraj
 *  \brief This class represents one trajectory.
 */
class TleTraj : public StelObject
{
    friend class TleTrajMgr;
    friend class TleFile;

public:
    TleTraj();
    virtual ~TleTraj();

    virtual QString getType(void) const
    {
        return "TleTraj";
    }
    virtual float getSelectPriority(const StelCore */*nav*/) const
    {
        return -10.;
    }
    virtual QString getNameI18n() const
    {
        return name;
    }
    virtual QString getEnglishName() const
    {
        return name;
    }
    virtual QString getInfoString(const StelCore *core,
                                  const InfoStringGroup &flags) const;
    virtual Vec3d getJ2000EquatorialPos(const StelCore *core) const;
    virtual double getAngularSize(const StelCore *core) const;
    void update(void);

    bool isInitialized;
    bool isVisible;
    QColor *orbitColor;
    QString name;

private:
    double curTime_utc; // Current JD
    sat_D curData;      // Object current data
    Vec3d XYZ;          // holds J2000 position
    double lastEvalTime;
    QList<Vec3d> trajectory; // trajectory points
    tle_t *tle; // object's TLE

    void draw(const StelCore *core, StelPainter &painter);
    void computeOrbitPoints(void);
    void satd2StelCoord(const sat_D &src, Vec3d &out) const;
    void drawOrbit(StelPainter &painter);
    void recalculateOrbitLines(void);

    static uint timeWindow;
    static StelLocation location;
    static StelTextureSP hintTexture;
    static uint orbitLineSegments;
    static SphericalCap viewportHalfspace;
};

#endif /* _TLETRAJ_HPP_ */
