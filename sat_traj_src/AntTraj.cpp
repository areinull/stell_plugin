#include "AntTraj.hpp"
#include "SatTrajMgr.hpp"
#include "StelApp.hpp"
#include "StelProjector.hpp"
#include "StelPainter.hpp"
#include "StelTexture.hpp"
#include "StelModuleMgr.hpp"
#include <QtOpenGL/QtOpenGL>
#include <shmci/shm_addr.h>
#include <inter_cn/structs.h>
#include <coord_conv/CoordConv.h>

const float AntTraj::pointerChangePerc = 0.05f;

AntTraj::AntTraj(QString id, QString texPath, int typ, SatTrajMgr& mgr, const QColorP& c)
  : GenTraj(id, texPath, typ, mgr, c)
  , shmUpdThread(0)
{
    memset(&shmCont, 0, sizeof shmCont);
}

AntTraj::~AntTraj()
{
    if (shmUpdThread)
    {
        deinitShmThread();
    }
}

void AntTraj::draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter)
{
    genDraw(mgr, painter);

    // Отрисовка значков положения
    const float fov = prj->getFov();
    if (antennaCone/fov > pointerChangePerc)
    {
        Vec3d xy;
        if (prj->project(XYZ, xy))
        {
            float rad = antennaCone/180.f*3.1416f*
                        prj->getPixelPerRadAtCenter()/2.f;
            glLineWidth(1);
            painter.drawCircle(xy[0], xy[1], rad);
        }
    }
    else if (!hintTexture.isNull())
    {
        painter.enableTexture2d(true);
        hintTexture->bind();
        Vec3d xy;
        if (prj->project(XYZ, xy))
        {
            painter.drawSprite2dMode(xy[0], xy[1], 16.f);
        }
        painter.enableTexture2d(false);
    }
}

void AntTraj::baseUpdate(void )
{
    SatTrajMgr* mgr = GETSTELMODULE(SatTrajMgr);
    if (!mgr->enableShm)
    {
        if (shmUpdThread)
        {
            deinitShmThread();
        }
        GenTraj::baseUpdate();
    }
    else if (!shmUpdThread)
    {
        initShmThread();
    }
}

void AntTraj::initShmThread(void )
{
//     qDebug() << "initShmThread() start";
    if (pthread_create(&shmUpdThread, 0, callShmRoutine, this))
    {
        qWarning() << "AntTraj pthread_create() " << strerror(errno);
    }
    pthread_spin_lock(&ptrChangeLock);
    trajDraw.clear();
    trajUpd.clear();
    pthread_spin_unlock(&ptrChangeLock);
}

void AntTraj::deinitShmThread(void )
{
//     qDebug() << "deinitShmThread() start";
    pthread_cancel(shmUpdThread);
    pthread_join(shmUpdThread, 0);
    shmUpdThread = 0;
    pthread_spin_lock(&ptrChangeLock);
    trajDraw.clear();
    trajUpd.clear();
    lastIdDraw = 0;
    lastIdUpd = 0;
    updReq = true;
    pthread_spin_unlock(&ptrChangeLock);
//     qDebug() << "deinitShmThread() end";
}

void* AntTraj::shmRoutine(void )
{
//     qDebug() << "shmRoutine() start";
    timespec to = {1,0};
    pthread_cleanup_push(callShmCleanup, this);

    int err = shm_sbuf_init(&shmCont, SHM_ADDR_ADRIVE_INTER, ADRIVE_BUF_CAPACITY,
                            sizeof(adrive_ext_pac_t));
    if (err < 0)
    {
        qWarning() << "AntTraj Shm buffer init failed with " << err << " - " <<
                      shm_sbuf_error(err);
        if ((err == (int)SHM_ERR) || (err == (int)SEM_ERR))
        {
            qWarning() << "msg from shmsbuf - " << shmCont.err_msg;
        }
    }
    else if (err != (int)CONN_TO_EXISTING)
    {
        qWarning() << "AntTraj Shm buffer init success with " << err << ", " <<
                      (int)CONN_TO_EXISTING << " expected";
    }
    else while (true)
    {
//         qDebug() << "shmRoutine() loop";
        adrive_ext_pac_t buf[ADRIVE_BUF_CAPACITY];
        int pointsReceived = shm_sbuf_nread(&shmCont, &buf, &to);
        if (pointsReceived > 0)
        {
//             qDebug() << "pointsReceived " << pointsReceived;
            QList<DataPoint> newPoints;
            newPoints.reserve(pointsReceived);
            for (register int i = 0; i < pointsReceived; ++i)
            {
                newPoints.append(DataPoint());
                newPoints[i].time = buf[i].ts;
                newPoints[i].dist = 0.;
                int_point ant_point;
                ant_point.first = *(s32*)(&buf[i].Az);
                ant_point.second = *(s32*)(&buf[i].El);
                double_point rls_point = antenna2radar(ant_point);
                // фикс для расчёта тангенса около пи/2
                if (rls_point.second >= M_PI_2 - 1e-9)
                {
                    rls_point.second -= 1e-9;
                }
                else if (rls_point.second <= -M_PI_2 + 1e-9)
                {
                    rls_point.second += 1e-9;
                }
                newPoints[i].vec = Vec3d(-cos(rls_point.first), sin(rls_point.first),
                                tan(rls_point.second));
                newPoints[i].vec.normalize();
            }
            // now add new points to draw vector
            pthread_spin_lock(&ptrChangeLock);
            trajDraw += newPoints;
            pthread_spin_unlock(&ptrChangeLock);
        }
        else
        {
            qWarning() << "AntTraj shm_sbuf_nread() returned " << pointsReceived;
            if ((pointsReceived == (int)SHM_ERR) ||
                (pointsReceived == (int)SEM_ERR))
            {
                qWarning() << "msg from shmsbuf - " << shmCont.err_msg;
            }
        }

        pthread_testcancel();
    }

    pthread_cleanup_pop(1);
//     qDebug() << "shmRoutine() end";
    return 0;
}

void AntTraj::shmRoutCleanup(void )
{
//     qDebug() << "shmRoutCleanup()";
    shm_sbuf_deinit(&shmCont);
    SatTrajMgr *mgr = GETSTELMODULE(SatTrajMgr);
    if (mgr)
        mgr->setEnableShm(false);
}
