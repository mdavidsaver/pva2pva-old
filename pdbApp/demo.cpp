
#include <stdexcept>
#include <memory>

#include <stdio.h>
#include <string.h>

#include <epicsMath.h>
#include <dbStaticLib.h>
#include <dbAccess.h>
#include <dbScan.h>
#include <recGbl.h>
#include <alarm.h>
#include <errlog.h>

#include <epicsStdio.h>

#include <waveformRecord.h>
#include <longoutRecord.h>
#include <menuFtype.h>

#include <epicsExport.h>

namespace {

// pi/180
static const double pi_180 = 0.017453292519943295;

int dummy;

long init_spin(waveformRecord *prec)
{
    if(prec->ftvl==menuFtypeDOUBLE)
        prec->dpvt = &dummy;
    return 0;
}

long process_spin(waveformRecord *prec)
{
    if(prec->dpvt != &dummy) {
        (void)recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
        return 0;
    }

    const double freq = 360.0*pi_180/100; // rad/sample
    double phase = 0;
    double *val = static_cast<double*>(prec->bptr);

    long ret = dbGetLink(&prec->inp, DBF_DOUBLE, &phase, 0, 0);
    if(ret) {
        (void)recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
        return ret;
    }

    phase *= pi_180; // deg -> rad

    for(size_t i=0, N=prec->nelm; i<N; i++)
        val[i] = sin(freq*i+phase);

    prec->nord = prec->nelm;

    return 0;
}

struct tag_pvt {
    epicsUInt64 tag;
    tag_pvt() :tag(0) {}
};

long init_tag(longoutRecord *prec)
{
    try {
        std::auto_ptr<tag_pvt> pvt(new tag_pvt);

        DBENTRY ent;

        dbInitEntry(pdbbase, &ent);

        if(dbFindRecord(&ent, prec->name)!=0)
            throw std::logic_error("Record not found");

        if(!dbFindInfo(&ent, "Q:time:tag") &&
           strcmp(dbGetInfoString(&ent), "usertag")==0) {
            // store a pointer to where we put the tag value
            dbPutInfoPointer(&ent, &pvt->tag);
        }

        dbFinishEntry(&ent);

        prec->dpvt = pvt.release();
    }catch(std::exception& e){
        fprintf(stderr, "%s : init error : %s\n", prec->name, e.what());
    }
    return 0;
}

long write_tag(longoutRecord *prec)
{
    tag_pvt *pvt = (tag_pvt*)prec->dpvt;
    if(!pvt) {
        (void)recGblSetSevr(prec, COMM_ALARM, INVALID_ALARM);
        return -1;
    }
    try {
        // fake a pulse id using VAL
        // repeat it to show that 64-bits are actually being stored and transported
        pvt->tag = prec->val;
        pvt->tag <<= 32;
        pvt->tag |= prec->val;

        dbPutLink(&prec->out, DBF_LONG, &prec->val, 1);
        return 0;
    }catch(std::exception& e){
        (void)recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        errlogPrintf("%s : error : %s\n", prec->name, e.what());
        return -1;
    }
}

template<typename REC>
struct dset5
{
    long count;
    long (*report)(int);
    long (*init)(int);
    long (*init_record)(REC *);
    long (*get_ioint_info)(int, REC *, IOSCANPVT*);
    long (*process)(REC *);
};

dset5<waveformRecord> devWfPDBDemo = {5,0,0,&init_spin,0,&process_spin};
dset5<longoutRecord> devLoPDBDemoTag = {5,0,0,&init_tag,0,&write_tag};

} // namespace

extern "C" {
epicsExportAddress(dset, devWfPDBDemo);
epicsExportAddress(dset, devLoPDBDemoTag);
}
