#include "GoodSample.hpp"
#include "StelApp.hpp"
#include "StelTextureMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "SatTrajMgr.hpp"

StelTextureSP GoodSample::hintTexture;
xNtpTime GoodSample::drawTime;

GoodSample::GoodSample(QString id, const QColorP& c,
    xNtpTime _t, const Vec3d &_pos, double _d, double dAz, double dEl)
  : GenObject(id, c)
  , t(_t)
  , deltaAz(dAz)
  , deltaEl(dEl)
{
    XYZ = _pos;
    curRange = _d;
    initialized = true;
}

GoodSample::~GoodSample()
{
}

void GoodSample::draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter)
{
    if (hintTexture.isNull() || !mgr->enableGoodSamples)
        return;

    xNtpTime l_time = drawTime - xNtpTime((u64)mgr->timeWindow << 32);
    xNtpTime r_time = drawTime + xNtpTime((u64)mgr->timeWindow << 32);
    if (t < l_time || t > r_time)
    {
        visible = false;
        return;
    }
    else
        visible = true;

    float hintSize = antennaCone*measurementPerc*2.f/180.f*3.1416f*
                     prj->getPixelPerRadAtCenter();
    if (hintSize < 8.f)
        hintSize = 8.f;
    else if (hintSize > 32.f)
        hintSize = 32.f;
    painter.enableTexture2d(true);
    painter.setColor(color->redF(), color->greenF(), color->blueF());
    hintTexture->bind();
    Vec3d xy;
    if (prj->project(XYZ, xy))
    {
        painter.drawSprite2dMode(xy[0], xy[1], hintSize);
    }
    painter.enableTexture2d(false);
}

QString GoodSample::getInfoString(const StelCore* core,
                                  const StelObject::InfoStringGroup& flags) const
{
    QString str;
    QTextStream oss(&str);

    if (flags&Name)
    {
        oss << "<h2>" << name << "</h2><br>";
    }

    oss << getPositionInfoString(core, flags);

    if (flags&Extra1)
    {
        oss << QString("Range (km): <b>%1</b>").arg(curRange/1e3, 0, 'f') << "<br>";
        oss << QString("dAz (ang min): <b>%1</b>").arg(deltaAz, 0, 'f') << "<br>";
        oss << QString("dEl (ang min): <b>%1</b>").arg(deltaEl, 0, 'f') << "<br>";
    }

    postProcessInfoString(str, flags);
    return str;
}
