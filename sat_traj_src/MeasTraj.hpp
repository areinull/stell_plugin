#ifndef _MEASTRAJ_HPP_
#define _MEASTRAJ_HPP_

#include "GenTraj.hpp"
#include <shmci/shmsbuf.h>

class MeasTraj : public GenTraj
{
public:
    MeasTraj(QString id, QString texPath, int typ, SatTrajMgr& mgr, const QColorP& c);
    virtual ~MeasTraj();

    virtual QString getType(void) const {return "MeasTraj";}
    virtual void draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter);
    virtual void baseUpdate(void);

private:
    pthread_t shmUpdThread;
    ShmSBuf shmCont;

    void* shmRoutine(void);
    void shmRoutCleanup(void);
    void deinitShmThread(void);
    void initShmThread(void);

    static void* callShmRoutine(void *arg) {return ((MeasTraj*)arg)->shmRoutine();}
    static void callShmCleanup(void *arg) {((MeasTraj*)arg)->shmRoutCleanup();}
};

#endif // _MEASTRAJ_HPP_
