#ifndef UTILITIES_H
#define UTILITIES_H

#include <deque>
#include <sstream>

#include <errlog.h>
#include <epicsEvent.h>
#include <epicsUnitTest.h>
#include <dbUnitTest.h>

#include <pv/pvUnitTest.h>
#include <pv/pvAccess.h>

#include "pvahelper.h"
#include "weakmap.h"
#include "weakset.h"

#include <shareLib.h>

struct TestPV;
struct TestPVChannel;
struct TestPVMonitor;
struct TestProvider;

// minimally useful boilerplate which must appear *everywhere*
#define DUMBREQUESTER(NAME) \
    virtual std::string getRequesterName() OVERRIDE { return #NAME; }

template<typename T>
inline std::string toString(const T& tbs)
{
    std::ostringstream oss;
    oss << tbs;
    return oss.str();
}

// Boilerplate reduction for accessing a scalar field
template<typename T>
struct ScalarAccessor {
    epics::pvData::PVScalar::shared_pointer field;
    typedef T value_type;
    ScalarAccessor(const epics::pvData::PVStructurePtr& s, const char *name)
        :field(s->getSubFieldT<epics::pvData::PVScalar>(name))
    {}
    operator value_type() {
        return field->getAs<T>();
    }
    ScalarAccessor& operator=(T v) {
        field->putFrom<T>(v);
        return *this;
    }
    ScalarAccessor& operator+=(T v) {
        field->putFrom<T>(field->getAs<T>()+v);
        return *this;
    }
};

struct TestIOC {
    bool hasInit;
    TestIOC() : hasInit(false) {
        testdbPrepare();
    }
    ~TestIOC() {
        this->shutdown();
        testdbCleanup();
    }
    void init() {
        if(!hasInit) {
            eltc(0);
            testIocInitOk();
            eltc(1);
            hasInit = true;
        }
    }
    void shutdown() {
        if(hasInit) {
            testIocShutdownOk();
            hasInit = false;
        }
    }
};

#endif // UTILITIES_H
