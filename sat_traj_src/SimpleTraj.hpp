#ifndef _SIMPLETRAJ_HPP_
#define _SIMPLETRAJ_HPP_

#include "GenTraj.hpp"


class SimpleTraj: public GenTraj
{
public:
    SimpleTraj(QString id, QString texPath, int typ, SatTrajMgr& mgr, const QColorP& c);
    virtual ~SimpleTraj();

    virtual QString getType(void) const {return "SimpleTraj";}
    virtual void draw(SatTrajMgr* mgr, StelProjectorP prj, StelPainter& painter);
};

#endif // _SIMPLETRAJ_HPP_
