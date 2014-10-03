#include "xNtpTime.hpp"
#include <math.h>

xNtpTime::xNtpTime(const ntptime_t &t):
    time(((uint64_t)t.sec<<32)|(uint64_t)t.psec) {}

xNtpTime::xNtpTime(const double &t)
{
    double tmp_sec;
    double tmp_frac = modf(t, &tmp_sec);
    time = ((uint64_t)tmp_sec << 32) | (uint64_t)(tmp_frac * 4294967296.);
}

inline void xNtpTime::ntpt(ntptime_t *dst) const
{
    if (dst)
    {
        dst->psec = psec();
        dst->sec = sec();
    }
}

bool operator<(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return lhs.ext() < rhs.ext();
}

bool operator>(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return lhs.ext() > rhs.ext();
}

bool operator<=(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return lhs.ext() <= rhs.ext();
}

bool operator>=(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return lhs.ext() >= rhs.ext();
}

bool operator==(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return lhs.ext() == rhs.ext();
}

bool operator!=(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return lhs.ext() != rhs.ext();
}

xNtpTime operator+(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return xNtpTime(lhs.ext() + rhs.ext());
}

xNtpTime operator-(const xNtpTime& lhs, const xNtpTime& rhs)
{
    return xNtpTime(lhs.ext() - rhs.ext());
}
