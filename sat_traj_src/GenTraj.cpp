#include "StelPainter.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocaleMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelTextureMgr.hpp"
#include "StelVertexArray.hpp"
#include "StelUtils.hpp"
#include "GenTraj.hpp"
#include "SatTrajMgr.hpp"

#include <QtOpenGL/QtOpenGL>
#include <stdexcept>
#include <inttypes.h>
#include <vu_tools/vu_tools.h>

GenTraj::GenTraj(QString id, QString texPath, int typ, SatTrajMgr& mgr,
                 const QColorP& c)
  : GenObject(id, c)
  , updReq(true)
  , lastIdDraw(-1)
  , lastIdUpd(-1)
  , type(typ)
  , mysql(&mgr.mysqlSecond)
  , antExtr(mgr.antExtr)
  , antExtrTime(mgr.antExtrTime)
{
    pthread_spin_init(&ptrChangeLock, 0);
    if (!texPath.isEmpty())
        hintTexture = StelApp::getInstance().getTextureManager().createTexture(texPath);
    initialized = true;
}

GenTraj::~GenTraj()
{
    qDebug() << "type "<< type << "number of points " << trajDraw.count();
    pthread_spin_destroy(&ptrChangeLock);
    trajDraw.clear();
    trajUpd.clear();
    mysql = NULL;
}

void GenTraj::genDraw(SatTrajMgr* mgr, StelPainter& painter)
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
    bool setCurP = false;
    int curP=-1;
    StelVertexArray trDraw(StelVertexArray::LineStrip);

    // Проверка хвостов траекторий на принадлежность окну отрисовки
    getStelTimeNTP(stelT);
    xNtpTime l_time = stelT - xNtpTime((u64)mgr->timeWindow << 32);
    xNtpTime r_time = stelT + xNtpTime((u64)mgr->timeWindow << 32);
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

    // Отбор отрисовываемых точек
    for (int i = trajDraw.size() - 1; i > 0; --i)
    {
        if ((trajDraw[i].time > r_time) || (trajDraw[i].time < l_time))
            continue;
        if (!setCurP && trajDraw[i].time > stelT && trajDraw[i-1].time <= stelT)
        {
            curP = i-1;
            setCurP = true;
        }
        trDraw.vertex.append(trajDraw[i].vec);
    }
    visible = true;

    if (!setCurP)
    {   // Точка текущего положения берётся с края массива
        if (trajDraw[0].time > stelT)
        {
            curP = 0;
            XYZ = trajDraw[curP].vec;
        }
        else
        {
            curP = trajDraw.size() - 1;
            // применить экстраполяцию отображаемой траектории
            if (antExtr && (getType() == "AntTraj") && (trajDraw.size() > 2))
            {
                Vec3d tmp;
                tmp[0] = trajDraw[curP].vec[0] +
                    (trajDraw[curP].vec[0]-trajDraw[curP-1].vec[0])/
                    (trajDraw[curP].time-trajDraw[curP-1].time).doub()*
                    antExtrTime;
                tmp[1] = trajDraw[curP].vec[1] +
                    (trajDraw[curP].vec[1]-trajDraw[curP-1].vec[1])/
                    (trajDraw[curP].time-trajDraw[curP-1].time).doub()*
                    antExtrTime;
                tmp[2] = trajDraw[curP].vec[2] +
                    (trajDraw[curP].vec[2]-trajDraw[curP-1].vec[2])/
                    (trajDraw[curP].time-trajDraw[curP-1].time).doub()*
                    antExtrTime;
                tmp.normalize();
                trDraw.vertex.prepend(tmp);

                uint64_t t_down, t_up, t_t;
                double a, b;
                t_down = trajDraw[curP].time.ext();
                t_up = (trajDraw[curP].time + antExtrTime).ext();
                t_t = stelT.ext();
                a = (double)(t_t - t_down)/(double)(t_up - t_down);
                b = (double)(t_up - t_t)/(double)(t_up - t_down);
                XYZ[0] = trajDraw[curP].vec[0]*b + tmp[0]*a;
                XYZ[1] = trajDraw[curP].vec[1]*b + tmp[1]*a;
                XYZ[2] = trajDraw[curP].vec[2]*b + tmp[2]*a;
            }
            else
                XYZ = trajDraw[curP].vec;
        }
    }
    else
    {
        uint64_t t_down, t_up, t_t;
        double a, b;
        t_down = trajDraw[curP].time.ext();
        t_up = trajDraw[curP+1].time.ext();
        t_t = stelT.ext();
        a = (double)(t_t - t_down)/(double)(t_up - t_down);
        b = (double)(t_up - t_t)/(double)(t_up - t_down);
        XYZ[0] = trajDraw[curP].vec[0]*b + trajDraw[curP+1].vec[0]*a;
        XYZ[1] = trajDraw[curP].vec[1]*b + trajDraw[curP+1].vec[1]*a;
        XYZ[2] = trajDraw[curP].vec[2]*b + trajDraw[curP+1].vec[2]*a;
    }
    curRange = trajDraw[curP].dist;
    pthread_spin_unlock(&ptrChangeLock);

    // Отрисовка
    if (!trDraw.vertex.isEmpty())
    {
        glLineWidth(1);
        painter.drawStelVertexArray(trDraw);
    }
}

void GenTraj::baseUpdate(void)
{
    if (!updReq)
        return;

    SatTrajMgr* mgr = GETSTELMODULE(SatTrajMgr);
    xNtpTime stelT, l_time, r_time, ld_time, rd_time;
    bool r_req = false, l_req = false;
    char query[4096];

    getStelTimeNTP(stelT);
    if (mgr->timeWindow < 300)
    {
        l_time = stelT - 300.;
        r_time = stelT + 300.;
    }
    else
    {
        l_time = stelT - xNtpTime((u64)(mgr->timeWindow * wndRelSize) << 32);
        r_time = stelT + xNtpTime((u64)(mgr->timeWindow * wndRelSize) << 32);
    }
    ld_time = stelT - xNtpTime((u64)mgr->timeWindow << 32);
    rd_time = stelT + xNtpTime((u64)mgr->timeWindow << 32);
    // Удаление лишних точек
    if (!trajUpd.isEmpty())
    {
        // поиск слева
        QList<DataPoint>::iterator l_it = trajUpd.begin(),
                                   r_it = trajUpd.end();
        for (; l_it != r_it; ++l_it)
        {
            if (l_it->time > l_time)
                break;
        }
        if (l_it != trajUpd.begin())
        {
            trajUpd.erase(trajUpd.begin(), l_it);
//             qDebug() << "left erased";
        }
    }
    if (!trajUpd.isEmpty())
    {
        // поиск справа
        QList<DataPoint>::iterator l_it = trajUpd.begin(),
                                   r_it = trajUpd.end() - 1;
        for (; r_it != l_it; --r_it)
        {
            if (r_it->time < r_time)
                break;
        }
        if (r_it != trajUpd.end() - 1)
        {
            trajUpd.erase(++r_it, trajUpd.end());
//             qDebug() << "right erased";
        }
    }
    // Проверка хвостов
    if (!trajUpd.isEmpty() && (trajUpd.last().time < rd_time))
    {
        r_req = true;
//             qDebug() << "right side is needed";
    }
    if (!trajUpd.isEmpty() && (trajUpd.first().time > ld_time))
    {
        l_req = true;
//             qDebug() << "left side is needed";
    }
    if (trajUpd.isEmpty())
    {   // Запрос всего окна по времени
//         qDebug() << "full window req";
        if (getType() == "MeasTraj")
        {
            snprintf(query, 4096,
                     "SELECT Time,pAz,pUm,Dist,ID FROM %s WHERE Time>%"PRIu64" AND "
                     "Time<%"PRIu64" ORDER BY ID ASC",
                     "InterCnTrack", l_time.ext(), r_time.ext());
        }
        else
        {
            snprintf(query, 4096,
                     "SELECT Time,Az,Um,Dist,ID FROM %s WHERE Type=%d AND "
                     "Time>%"PRIu64" AND Time<%"PRIu64" ORDER BY ID ASC",
                     mgr->tableName.c_str(), type, l_time.ext(), r_time.ext());
        }
        sendParseQuery(query, mgr);
    }
    else
    {
        if (r_req)
        {   // запрос правой части окна
//             qDebug() << "right part req";
            if (getType() == "MeasTraj")
            {
                snprintf(query, 4096,
                         "SELECT Time,pAz,pUm,Dist,ID FROM %1$s WHERE ID>%2$d AND "
                         "Time<%4$"PRIu64" OR Time>%3$"PRIu64" AND "
                         "Time<%4$"PRIu64" ORDER BY ID ASC",
                         "InterCnTrack", lastIdUpd, trajUpd.last().time.ext(),
                         r_time.ext());
            }
            else
            {
                snprintf(query, 4096,
                         "SELECT Time,Az,Um,Dist,ID FROM %1$s WHERE Type=%2$d "
                         "AND (ID>%3$d AND Time<%5$"PRIu64" OR "
                         "Time>%4$"PRIu64" AND Time<%5$"PRIu64") ORDER BY ID ASC",
                         mgr->tableName.c_str(), type, lastIdUpd,
                         trajUpd.last().time.ext(), r_time.ext());
            }
            sendParseQuery(query, mgr);
        }
        if (l_req)
        {   // запрос левой части окна
//             qDebug() << "left part req";
            if (getType() == "MeasTraj")
            {
                snprintf(query, 4096,
                         "SELECT Time,pAz,pUm,Dist,ID FROM %s WHERE "
                         "Time>%"PRIu64" AND Time<%"PRIu64" ORDER BY ID DESC",
                         "InterCnTrack", l_time.ext(), trajUpd.first().time.ext());
            }
            else
            {
                snprintf(query, 4096,
                         "SELECT Time,Az,Um,Dist,ID FROM %s WHERE Type=%d AND "
                         "Time>%"PRIu64" AND Time<%"PRIu64" ORDER BY ID DESC",
                         mgr->tableName.c_str(), type, l_time.ext(),
                         trajUpd.first().time.ext());
            }
            sendParseQuery(query, mgr, true);
        }
    }
    cleanupTraj(trajUpd);
    // now swap traj vectors
    pthread_spin_lock(&ptrChangeLock);
    int tmp = lastIdDraw;
    lastIdDraw = lastIdUpd;
    lastIdUpd = tmp;
    trajDraw.swap(trajUpd);
    updReq = false;
    pthread_spin_unlock(&ptrChangeLock);
}

void GenTraj::cleanupTraj(QList<DataPoint> &traj)
{
    if (traj.isEmpty())
        return;
    xNtpTime lastGood = traj.last().time;
    for (int i = traj.size() - 1; i >= 0; --i)
    {
        if (traj[i].time > lastGood)
        {
            traj.removeAt(i);
        }
        else
        {
            lastGood = traj[i].time;
        }
    }
}

void GenTraj::sendParseQuery(const char query[], const SatTrajMgr* mgr,
                             bool prepend)
{
    const double min_change = 1.2e-4*1.2e-4;
    static double prevAz = 0., prevEl = 0.;
    double curAz, curEl;
    DataPoint tmp;
    MYSQL_RES *pRes;
    MYSQL_ROW row;

    if (mysql_query(mysql, query))
    {
        char buf[1024];
        snprintf(buf, 1024, "[%s:%d] Error: can't send query to server:\n",
                 _FILE_, __LINE__);
        std::string err(buf);
        err += mysql_error(mysql);
        throw std::runtime_error(err);
    }
    pRes = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(pRes)))
    {
        lastIdUpd = atoi(row[4]);
        if (mgr->changeDecimal)
        {
            for (register uint i = 0; i < sizeof(row[1]); ++i)
                if (row[1][i] == '.')
                    row[1][i] = ',';
            for (register uint i = 0; i < sizeof(row[2]); ++i)
                if (row[2][i] == '.')
                    row[2][i] = ',';
        }
        curAz = atof(row[1]);
        curEl = atof(row[2]);
        // фикс для расчёта тангенса около пи/2
        if (curEl >= M_PI_2 - 1e-9)
        {
            curEl -= 1e-9;
        }
        else if (curEl <= -M_PI_2 + 1e-9)
        {
            curEl += 1e-9;
        }
        // отброс близкорасположенных точек
        if (true && (getType() != "MeasTraj"))
        {
            if (!trajUpd.isEmpty() &&
                (((curAz-prevAz)*(curAz-prevAz)+
                  (curEl-prevEl)*(curEl-prevEl)) < min_change))
                continue;
            prevAz = curAz;
            prevEl = curEl;
        }
        tmp.time = (u64)strtoull(row[0], NULL, 10);
        tmp.vec = Vec3d(-cos(curAz), sin(curAz), tan(curEl));
        tmp.vec.normalize();
        tmp.dist = atof(row[3]);
        if (prepend)
            trajUpd.prepend(tmp);
        else
            trajUpd.append(tmp);
    } // while
    mysql_free_result(pRes);
}

QString GenTraj::getInfoString(const StelCore *core,
                               const InfoStringGroup& flags) const
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
    oss << QString("Range (km): <b>%1</b>").arg(curRange/1e3, 0, 'f') << "<br>";
  }

  postProcessInfoString(str, flags);
  return str;
}

DataPoint GenTraj::findByTime(xNtpTime t)
{
    pthread_spin_lock(&ptrChangeLock);
    if (trajDraw.isEmpty())
    {
        pthread_spin_unlock(&ptrChangeLock);
        return DataPoint();
    }

    for (int i = trajDraw.size() - 1; i > 0; --i)
    {
        if (trajDraw[i-1].time <= t && t < trajDraw[i].time)
        {
            DataPoint ret;
            // линейная интерполяция
            uint64_t t_down, t_up;
            double a, b;
            t_down = trajDraw[i-1].time.ext();
            t_up = trajDraw[i].time.ext();
            a = (double)(t.ext() - t_down)/(double)(t_up - t_down);
            b = (double)(t_up - t.ext())/(double)(t_up - t_down);
            ret.vec[0] = trajDraw[i-1].vec[0]*b + trajDraw[i].vec[0]*a;
            ret.vec[1] = trajDraw[i-1].vec[1]*b + trajDraw[i].vec[1]*a;
            ret.vec[2] = trajDraw[i-1].vec[2]*b + trajDraw[i].vec[2]*a;
            ret.time = t;
            ret.dist = trajDraw[i-1].dist*b + trajDraw[i].dist*a;

            pthread_spin_unlock(&ptrChangeLock);
            return ret;
        }
    }

    pthread_spin_unlock(&ptrChangeLock);
    return DataPoint();
}
