#include "SimpleTraj.hpp"
#include "StelProjector.hpp"
#include "StelTexture.hpp"
#include "StelPainter.hpp"

SimpleTraj::SimpleTraj(QString id, QString texPath, int typ, SatTrajMgr& mgr,
                       const QColorP& c)
  : GenTraj(id, texPath, typ, mgr, c)
{

}

SimpleTraj::~SimpleTraj()
{

}

void SimpleTraj::draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter)
{
    genDraw(mgr, painter);

    // Отрисовка значков положения
    if (hintTexture.isNull())
        return;
    painter.enableTexture2d(true);
    hintTexture->bind();
    Vec3d xy;
    if (prj->project(XYZ, xy))
    {
        painter.drawSprite2dMode(xy[0], xy[1], 16.f);
    }
    painter.enableTexture2d(false);
}
