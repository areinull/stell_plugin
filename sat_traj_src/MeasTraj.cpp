#include "MeasTraj.hpp"
#include "SatTrajMgr.hpp"
#include "StelApp.hpp"
#include "StelModuleMgr.hpp"
#include "StelPainter.hpp"
#include <QtOpenGL/QtOpenGL>
#include <shmci/shm_addr.h>
#include <inter_cn/structs.h>

MeasTraj::MeasTraj(QString id, QString texPath, int typ, SatTrajMgr& mgr, const QColorP& c)
  : GenTraj(id, texPath, typ, mgr, c)
  , shmUpdThread(0)
{
    memset(&shmCont, 0, sizeof shmCont);
}

MeasTraj::~MeasTraj()
{
    if (shmUpdThread)
    {
        deinitShmThread();
    }
}

void MeasTraj::draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter)
{
    pthread_spin_lock(&ptrChangeLock);
    if (trajDraw.isEmpty())
    {
        visible = false;
        updReq = true;
        pthread_spin_unlock(&ptrChangeLock);
        return;
    }

    xNtpTime stelT;
    StelVertexArray trDraw(StelVertexArray::Points);

    // Проверка хвостов траекторий на принадлежность окну отрисовки
    getStelTimeNTP(stelT);
    xNtpTime l_time = stelT - mgr->timeWindowSamples;
    xNtpTime r_time = stelT + mgr->timeWindowSamples;
    if (trajDraw.last().time < r_time)
    {
        updReq = true;
    }
    if (trajDraw.first().time > l_time)
    {
        updReq = true;
    }

    // Подготовка отрисовки
    painter.enableTexture2d(false);
    painter.setColor(color->redF(), color->greenF(), color->blueF());
    float pointSize = antennaCone*measurementPerc/180.f*3.1416f*
                      prj->getPixelPerRadAtCenter();
    if (pointSize < 4.f)
        pointSize = 4.f;
    painter.setPointSize(pointSize);
    glEnable(GL_POINT_SMOOTH);

    // Отбор отрисовываемых точек и собственно отрисовка
    for (int i = trajDraw.size() - 1; i > 0; --i)
    {
        if ((trajDraw[i].time > r_time) || (trajDraw[i].time < l_time))
            continue;

        trDraw.vertex.append(trajDraw[i].vec);
        float tdiff = (stelT.doub() - trajDraw[i].time.doub()) /
                      mgr->timeWindowSamples;
        if (0.f <= tdiff && tdiff < 1.f)
        {
            float alpha = (tdiff > 0.2f)? (1.f - tdiff)/0.8f: 1.f;
            painter.setColor(color->redF(), color->greenF(), color->blueF(),
                             alpha);
            painter.drawStelVertexArray(trDraw);
        }
        trDraw.vertex.pop_back();
    }

    visible = true;
    glDisable(GL_POINT_SMOOTH);
    pthread_spin_unlock(&ptrChangeLock);
}

void MeasTraj::baseUpdate(void )
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

void MeasTraj::initShmThread(void )
{
    if (pthread_create(&shmUpdThread, 0, callShmRoutine, this))
    {
        qWarning() << "MeasTraj pthread_create() " << strerror(errno);
    }
    pthread_spin_lock(&ptrChangeLock);
    trajDraw.clear();
    trajUpd.clear();
    pthread_spin_unlock(&ptrChangeLock);
}

void MeasTraj::deinitShmThread(void )
{
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
}

void MeasTraj::shmRoutCleanup(void )
{
    shm_sbuf_deinit(&shmCont);
    SatTrajMgr *mgr = GETSTELMODULE(SatTrajMgr);
    if (mgr)
        mgr->setEnableShm(false);
}

void* MeasTraj::shmRoutine(void )
{
    timespec to = {1,0};
    pthread_cleanup_push(callShmCleanup, this);

    int err = shm_sbuf_init(&shmCont, SHM_ADDR_INTER_BTO_SHMSBUF,
                            NeOdnDetectionSamplesBufferCapacity,
                            sizeof(NeOdnDetectionSampleType));
    if (err < 0)
    {
        qWarning() << "MeasTraj Shm buffer init failed with " << err << " - " <<
                      shm_sbuf_error(err);
        if ((err == (int)SHM_ERR) || (err == (int)SEM_ERR))
        {
            qWarning() << "msg from shmsbuf - " << shmCont.err_msg;
        }
    }
    else if (err != (int)CONN_TO_EXISTING)
    {
        qWarning() << "MeasTraj Shm buffer init success with " << err << ", " <<
                      (int)CONN_TO_EXISTING << " expected";
    }
    else while (true)
    {
        NeOdnDetectionSampleType buf[NeOdnDetectionSamplesBufferCapacity];
        int pointsReceived = shm_sbuf_nread(&shmCont, &buf, &to);
        if (pointsReceived > 0)
        {
            QList<DataPoint> newPoints;
            newPoints.reserve(pointsReceived);
            for (register int i = 0; i < pointsReceived; ++i)
            {
                newPoints.append(DataPoint());
                newPoints[i].time = buf[i].time;
                newPoints[i].dist = buf[i].D;
                // фикс для расчёта тангенса около пи/2
                if (buf[i].El >= M_PI_2 - 1e-9)
                {
                    buf[i].El -= 1e-9;
                }
                else if (buf[i].El <= -M_PI_2 + 1e-9)
                {
                    buf[i].El += 1e-9;
                }
                newPoints[i].vec = Vec3d(-cos(buf[i].Az), sin(buf[i].Az),
                                         tan(buf[i].El));
                newPoints[i].vec.normalize();
            }
            // now add new points to draw vector
            pthread_spin_lock(&ptrChangeLock);
            trajDraw += newPoints;
            pthread_spin_unlock(&ptrChangeLock);
        }
        else
        {
            qWarning() << "MeasTraj shm_sbuf_nread() returned " << pointsReceived;
            if ((pointsReceived == (int)SHM_ERR) ||
                (pointsReceived == (int)SEM_ERR))
            {
                qWarning() << "msg from shmsbuf - " << shmCont.err_msg;
            }
        }

        pthread_testcancel();
    }

    pthread_cleanup_pop(1);
    return 0;
}
