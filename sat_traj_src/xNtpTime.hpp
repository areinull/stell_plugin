#ifndef _XNTPTIME_HPP_
#define _XNTPTIME_HPP_

#include <QtCore/qglobal.h>
#include <ntptime/ntptime.h>

/*! \class xNtpTime
 *  \brief Wrapper class for ntptime_t and uint64_t.
 */
class xNtpTime
{
public:
    xNtpTime(): time(0) {}
    xNtpTime(const ntptime_t &t);
    xNtpTime(const uint64_t &t): time(t) {}
    xNtpTime(const double &);
    xNtpTime& operator+=(const xNtpTime &);
    uint64_t ext(void) const {return time;}
    void     ext(uint64_t *dst) const {if (dst) *dst = time;}
    uint32_t sec(void) const;
    void     sec(uint32_t *dst) const;
    uint32_t psec(void) const;
    void     psec(uint32_t *dst) const;
    void     ntpt(ntptime_t *dst) const;
    double   doub(void) const;
    void     doub(double *dst) const;
private:
    uint64_t time;
};
Q_DECLARE_TYPEINFO(xNtpTime, Q_PRIMITIVE_TYPE);

inline xNtpTime& xNtpTime::operator+=(const xNtpTime& rhs)
{
    time += rhs.ext();
    return *this;
}

inline uint32_t xNtpTime::sec(void) const
{
    return (uint32_t)(time >> 32);
}

inline void xNtpTime::sec(uint32_t *dst) const
{
    if (dst)
        *dst = (uint32_t)(time >> 32);
}

inline uint32_t xNtpTime::psec(void) const
{
    return (uint32_t)(time & 0xffffffff);
}

inline void xNtpTime::psec(uint32_t *dst) const
{
    if (dst)
        *dst = (uint32_t)(time & 0xffffffff);
}

inline double xNtpTime::doub(void) const
{
    return (double)sec() + (double)psec()*2.32830643654e-10;
}

inline void xNtpTime::doub(double *dst) const
{
    if (dst)
        *dst = (double)sec() + (double)psec()*2.32830643654e-10;
}

bool operator<(const xNtpTime &, const xNtpTime &);
bool operator>(const xNtpTime &, const xNtpTime &);
bool operator<=(const xNtpTime &, const xNtpTime &);
bool operator>=(const xNtpTime &, const xNtpTime &);
bool operator==(const xNtpTime &, const xNtpTime &);
bool operator!=(const xNtpTime &, const xNtpTime &);
xNtpTime operator+(const xNtpTime &, const xNtpTime &);
xNtpTime operator-(const xNtpTime &, const xNtpTime &);

#endif /* _XNTPTIME_HPP_ */
