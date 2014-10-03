#include "GenObject.hpp"
#include "xNtpTime.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"

const float GenObject::wndRelSize = 4.f;
const float GenObject::antennaCone = 4.f/60.f;
const float GenObject::measurementPerc = 0.1f;

void GenObject::getStelTimeNTP(xNtpTime &time)
{
    double tmp_time = StelApp::getInstance().getCore()->getJDay(); // JD
    // seconds in NTP epoch
    tmp_time = (tmp_time - 2440587.5) * 86400. + NTP_UNIX_DELTA;
    time = tmp_time;
}

Vec3d GenObject::getJ2000EquatorialPos(const StelCore *core) const
{
    return core->altAzToJ2000(XYZ);
}
