#ifndef _GENTRAJ_HPP_
#define _GENTRAJ_HPP_

#include "VecMath.hpp"
#include "xNtpTime.hpp"
#include "StelTextureTypes.hpp"
#include "GenObject.hpp"
#include <mysql/mysql.h>
#include <pthread.h>

class SatTrajMgr;
class StelPainter;

struct DataPoint
{
    DataPoint(void): time((uint64_t)0) {}

    xNtpTime time;
    Vec3d vec;
    double dist;
};
Q_DECLARE_TYPEINFO(DataPoint, Q_PRIMITIVE_TYPE);

class GenTraj : public GenObject
{
  public:
    GenTraj(QString id, QString texPath, int typ, SatTrajMgr &mgr, const QColorP& c);
    virtual ~GenTraj();

    virtual QString getType(void) const =0;
    virtual void draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter)=0;
    virtual QString getInfoString(const StelCore *core, const InfoStringGroup& flags) const;
    virtual void baseUpdate(void);

    DataPoint findByTime(xNtpTime t);

  protected:
    void genDraw(SatTrajMgr* mgr, StelPainter& painter);

    pthread_spinlock_t ptrChangeLock;
    bool updReq;
    StelTextureSP hintTexture;
    QList<DataPoint> trajDraw;
    QList<DataPoint> trajUpd;
    int lastIdDraw, lastIdUpd;

    // удаление перекрывающихся по времени участков
    void cleanupTraj(QList<DataPoint> &traj);

  private:
    int type; // используется при обращении к БД
    MYSQL *mysql;
    const bool &antExtr;
    const double &antExtrTime;

    void sendParseQuery(const char query[], const SatTrajMgr* mgr,
                        bool prepend = false);
};

#endif /* _GENTRAJ_HPP_ */
