#ifndef _GOODSAMPLE_HPP_
#define _GOODSAMPLE_HPP_

#include "GenObject.hpp"
#include "StelTextureTypes.hpp"
#include "xNtpTime.hpp"

class GoodSample : public GenObject
{
public:
    GoodSample(QString id, const QColorP& c, xNtpTime _t,
               const Vec3d &_pos, double _d, double dAz, double dEl);
    virtual ~GoodSample();

    virtual QString getType(void) const {return "GoodSample";}
    virtual QString getInfoString(const StelCore *core, const InfoStringGroup& flags) const;
    virtual void draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter);
    virtual void baseUpdate(void) {};

    // используется для указания текущего модельного времени при отрисовке,
    // чтобы не рассчитывать одно и то же время для каждой точки
    static xNtpTime drawTime;
    static StelTextureSP hintTexture;

private:
    const xNtpTime t;     // время привязки отсчёта
    const double deltaAz;   // отклонение по азимуту от ЦУ [угл.мин]
    const double deltaEl;   // отклонение по углу места от ЦУ [угл.мин]
};

#endif // _GOODSAMPLE_HPP_
