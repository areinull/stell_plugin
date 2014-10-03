#include "StelProjector.hpp"
#include "StelPainter.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelTextureMgr.hpp"
#include "StelUtils.hpp"
#include "TleTraj.hpp"
#include "TleTrajMgr.hpp"

#include <QtOpenGL/QtOpenGL>

uint TleTraj::timeWindow;
StelLocation TleTraj::location;
SphericalCap TleTraj::viewportHalfspace = SphericalCap();
uint TleTraj::orbitLineSegments;
StelTextureSP TleTraj::hintTexture;

TleTraj::TleTraj():
        isInitialized(false), isVisible(false), orbitColor(NULL), curTime_utc(0.),
        lastEvalTime(0.), tle(NULL)
{
    memset(&curData, 0, sizeof(curData));
//     qDebug() << "TleTraj inited";
}

TleTraj::~TleTraj()
{
//     qDebug() << "TleTraj deinited";
    tle = NULL;
    orbitColor = NULL;
}

void TleTraj::draw(const StelCore *core, StelPainter& painter)
{
    Vec3d pos;
    satd2StelCoord(curData, pos);
    XYZ = core->altAzToJ2000(pos);
    Vec3d xy;
    if (painter.getProjector()->project(pos, xy))
    {
        glEnable(GL_TEXTURE_2D);
        glColor3f(orbitColor->redF(), orbitColor->greenF(), orbitColor->blueF());
        hintTexture->bind();
        painter.drawSprite2dMode(xy[0], xy[1], 16);
        glDisable(GL_TEXTURE_2D);
        drawOrbit(painter);
    }
}

void TleTraj::drawOrbit(StelPainter &painter)
{
    StelVertexArray vertexArray(StelVertexArray::LineStrip);
    for (QList<Vec3d>::iterator it = trajectory.begin(); it != trajectory.end(); ++it)
    {
        vertexArray.vertex.append(*it);
    }
    painter.setColor(orbitColor->redF(), orbitColor->greenF(), orbitColor->blueF());
    painter.drawGreatCircleArcs(vertexArray, &viewportHalfspace);
}

QString TleTraj::getInfoString(const StelCore *core, const InfoStringGroup& flags) const
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
        oss << QString("Range (m): <b>%1</b>").arg(curData.d) << "<br>";
    }
    postProcessInfoString(str, flags);
    return str;
}

Vec3d TleTraj::getJ2000EquatorialPos(const StelCore* /*core*/) const
{
    return XYZ;
}

double TleTraj::getAngularSize(const StelCore*) const
{
    return 0.00001;
}

void TleTraj::update(void)
{
    curTime_utc = StelApp::getInstance().getCore()->getJDay();
    sat_position_JD(curTime_utc, location.latitude*M_PI / 180., location.longitude*M_PI / 180.,
                    location.altitude*1e-3, *tle, &curData);
    computeOrbitPoints();
}

void TleTraj::satd2StelCoord(const sat_D& src, Vec3d& out) const
{
    double a = src.Az - M_PI_2;
    out = Vec3d(sin(a), cos(a), tan(src.Um));
    out.normalize();
}

void TleTraj::computeOrbitPoints(void)
{
    double evalInterval = (double)timeWindow / ((double)orbitLineSegments * 24. * 60. * 60.); // in JD format
    double orbitHalfSpan = (double)timeWindow / (2. * 24. * 60. * 60.); // in JD format
    double evalTime;
    sat_D tmp_data;
    Vec3d tmp_vec;
    int diffSlots;

    if (trajectory.isEmpty())   // Setup trajectory
    {
        evalTime  = curTime_utc - orbitHalfSpan;
        for (uint i = 0; i <= orbitLineSegments; ++i)
        {
            sat_position_JD(evalTime, location.latitude*M_PI / 180., location.longitude*M_PI / 180.,
                            location.altitude*1e-3, *tle, &tmp_data);
            satd2StelCoord(tmp_data, tmp_vec);
            trajectory.append(tmp_vec);
            evalTime += evalInterval;
        }
        lastEvalTime = curTime_utc;
    }
    else if (curTime_utc > lastEvalTime)
    { // compute next orbit point when clock runs forward
        double diffTime = curTime_utc - lastEvalTime;
        diffSlots = (int)(diffTime * 24. * 60. * 60. * (double)orbitLineSegments / (double)timeWindow);
        if (diffSlots > 0)
        {
            if (diffSlots > (int)orbitLineSegments)
            {
                diffSlots = orbitLineSegments + 1;
                evalTime = curTime_utc - orbitHalfSpan;
            }
            else
            {
                evalTime   = lastEvalTime + orbitHalfSpan + evalInterval;
            }
            for (int i = 0; i < diffSlots; ++i)
            {  //remove points at beginning of list and add points at end.
                trajectory.removeFirst();
                sat_position_JD(evalTime, location.latitude*M_PI / 180., location.longitude*M_PI / 180.,
                                location.altitude*1e-3, *tle, &tmp_data);
                satd2StelCoord(tmp_data, tmp_vec);
                trajectory.append(tmp_vec);
                evalTime += evalInterval;
            }
            lastEvalTime = curTime_utc;
        }
    }
    else if (curTime_utc < lastEvalTime)
    { // compute next orbit point when clock runs backward
        double diffTime = lastEvalTime - curTime_utc;
        diffSlots = (int)(diffTime * 24. * 60. * 60. * (double)orbitLineSegments / (double)timeWindow);
        if (diffSlots > 0)
        {
            if (diffSlots > (int)orbitLineSegments)
            {
                diffSlots = orbitLineSegments + 1;
                evalTime   = curTime_utc + orbitHalfSpan;
            }
            else
            {
                evalTime   = curTime_utc - orbitHalfSpan - evalInterval;
            }
            for (int i = 0; i < diffSlots; ++i)
            { //remove points at end of list and add points at beginning.
                trajectory.removeLast();
                sat_position_JD(evalTime, location.latitude*M_PI / 180., location.longitude*M_PI / 180.,
                                location.altitude*1e-3, *tle, &tmp_data);
                satd2StelCoord(tmp_data, tmp_vec);
                trajectory.push_front(tmp_vec);
                evalTime -= evalInterval;
            }
            lastEvalTime = curTime_utc;
        }
    }
}

void TleTraj::recalculateOrbitLines(void)
{
    trajectory.clear();
}
