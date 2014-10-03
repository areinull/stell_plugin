#ifndef _ANTTRAJ_HPP_
#define _ANTTRAJ_HPP_

#include "GenTraj.hpp"
#include <shmci/shmsbuf.h>

class AntTraj : public GenTraj
{
public:
    AntTraj(QString id, QString texPath, int typ, SatTrajMgr& mgr, const QColorP& c);
    virtual ~AntTraj();

    virtual QString getType(void) const {return "AntTraj";}
    virtual void draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter);
    virtual void baseUpdate(void);

private:
    pthread_t shmUpdThread;
    ShmSBuf shmCont;

    void* shmRoutine(void);
    void shmRoutCleanup(void);
    void deinitShmThread(void);
    void initShmThread(void);

    // Percentage of FOV occupied by beam, when change to circle from texture
    static const float pointerChangePerc;

    static void* callShmRoutine(void *arg) {return ((AntTraj*)arg)->shmRoutine();}
    static void callShmCleanup(void *arg) {((AntTraj*)arg)->shmRoutCleanup();}
};

#endif // _ANTTRAJ_HPP_
