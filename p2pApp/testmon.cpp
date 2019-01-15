
#include <epicsAtomic.h>
#include <epicsGuard.h>
#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/epicsException.h>
#include <pv/monitor.h>
#include <pv/thread.h>
#include <pv/serverContext.h>
#include <pva/server.h>
#include <pva/sharedstate.h>
#include <pva/client.h>

#include "server.h"

#include "utilities.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;

namespace {

pvd::PVStructurePtr makeRequest(size_t bsize, bool pipeline=false)
{
    pvd::StructureConstPtr dtype(pvd::getFieldCreate()->createFieldBuilder()
                                 ->addNestedStructure("record")
                                    ->addNestedStructure("_options")
                                        ->add("queueSize", pvd::pvString) // yes, really.  PVA wants a string
                                        ->add("pipeline", pvd::pvBoolean)
                                    ->endNested()
                                 ->endNested()
                                 ->createStructure());

    pvd::PVStructurePtr ret(pvd::getPVDataCreate()->createPVStructure(dtype));
    ret->getSubFieldT<pvd::PVScalar>("record._options.queueSize")->putFrom<pvd::int32>(bsize);
    ret->getSubFieldT<pvd::PVBoolean>("record._options.pipeline")->put(pipeline);

    return ret;
}

struct XYRecord
{
    static const pvd::StructureConstPtr type;

    pvas::SharedPV::shared_pointer pv;

    pvd::int32 x, y;

    XYRecord()
        :pv(pvas::SharedPV::buildMailbox())
        ,x(0)
        ,y(0)
    {
        pv->open(type);
    }
    ~XYRecord()
    {
        pv->close(true);
    }

    void post(bool px, bool py)
    {
        pvd::PVStructurePtr val(type->build());
        pvd::BitSet changed;
        if(px) {
            pvd::PVScalarPtr fld(val->getSubFieldT<pvd::PVScalar>("x"));
            fld->putFrom(x);
            changed.set(fld->getFieldOffset());
        }
        if(py) {
            pvd::PVScalarPtr fld(val->getSubFieldT<pvd::PVScalar>("y"));
            fld->putFrom(y);
            changed.set(fld->getFieldOffset());
        }
        pv->post(*val, changed);
    }
};

const pvd::StructureConstPtr XYRecord::type(pvd::FieldBuilder::begin()
                                            ->add("x", pvd::pvInt)
                                            ->add("y", pvd::pvInt)
                                            ->createStructure());

struct TestMonitor {
    pvas::StaticProvider upstream;
    XYRecord test1;

    GWServerChannelProvider::shared_pointer gateway;

    pvac::ClientProvider client;
    pvac::ClientChannel chan;

    // prepare providers and connect the client channel, don't setup monitor
    TestMonitor()
        :upstream("upstream")
        ,gateway(new GWServerChannelProvider(upstream.provider()))
        ,client(gateway)
    {
        testDiag("pre-test setup");
        upstream.add("test1", test1.pv);

        test1.x = 1;
        test1.y = 2;
        test1.post(true, true);

        chan = client.connect("test1");
    }

    ~TestMonitor() {}

    void test_event()
    {
        testDiag("Push the initial event through from upstream to downstream");

        pvac::MonitorSync mon(chan.monitor(makeRequest(2)));

        testOk1(mon.wait(1.0));
        testOk1(mon.poll());

        testFieldEqual<pvd::PVInt>(mon.root, "x", 1);
        testFieldEqual<pvd::PVInt>(mon.root, "y", 2);
        testEqual(mon.changed, pvd::BitSet().set(0));
        testEqual(mon.overrun, pvd::BitSet());

        testOk1(!mon.poll());
        testOk1(!mon.wait(0.1)); // timeout
        testOk1(!mon.poll());
    }

    void test_share()
    {
        // here both downstream monitors are on the same Channel,
        // which would be inefficient, and slightly unrealistic, w/ real PVA,
        // but w/ SharedPV makes no difference
        testDiag("Test two downstream monitors sharing the same upstream");

        pvac::MonitorSync mon(chan.monitor(makeRequest(2)));
        pvac::MonitorSync mon2(chan.monitor(makeRequest(2)));

        testOk1(mon.wait(1.0));
        testOk1(mon2.wait(1.0));
        testOk1(mon.poll());
        testOk1(mon2.poll());

        testFieldEqual<pvd::PVInt>(mon.root, "x", 1);
        testFieldEqual<pvd::PVInt>(mon.root, "y", 2);
        testEqual(mon.changed, pvd::BitSet().set(0));
        testEqual(mon.overrun, pvd::BitSet());

        testFieldEqual<pvd::PVInt>(mon2.root, "x", 1);
        testFieldEqual<pvd::PVInt>(mon2.root, "y", 2);
        testEqual(mon2.changed, pvd::BitSet().set(0));
        testEqual(mon2.overrun, pvd::BitSet());


        testOk1(!mon.wait(0.1)); // timeout
        testOk1(!mon.poll());
        testOk1(!mon2.poll());

        testDiag("explicitly push an update");
        test1.x = 42;
        test1.y = 43;
        test1.post(true, false); // only indicate that 'x' changed

        testOk1(mon.wait(1.0));
        testOk1(mon2.wait(1.0));
        testOk1(mon.poll());
        testOk1(mon2.poll());

        testFieldEqual<pvd::PVInt>(mon.root, "x", 42);
        testFieldEqual<pvd::PVInt>(mon.root, "y", 2);
        testEqual(mon.changed, pvd::BitSet().set(1));
        testEqual(mon.overrun, pvd::BitSet());

        testFieldEqual<pvd::PVInt>(mon2.root, "x", 42);
        testFieldEqual<pvd::PVInt>(mon2.root, "y", 2);
        testEqual(mon2.changed, pvd::BitSet().set(1));
        testEqual(mon2.overrun, pvd::BitSet());

        testOk1(!mon.poll());
        testOk1(!mon2.poll());

        testOk1(!mon.wait(0.1)); // timeout
        testOk1(!mon.poll());
        testOk1(!mon2.poll());
    }

    void test_overflow_downstream()
    {
        testDiag("Check behavour when downstream monitor overflows");

        pvac::MonitorSync mon(chan.monitor(makeRequest(3)));

        testOk1(mon.wait(1.0));
        testOk1(mon.poll());

        test1.x = 50;
        test1.post(true, false);
        test1.x = 51;
        test1.post(true, false);
        test1.x = 52;
        test1.post(true, false);
        test1.x = 53;
        test1.post(true, false);

        testOk1(mon.wait(1.0));
        testOk1(mon.poll());
        testFieldEqual<pvd::PVInt>(mon.root, "x", 50);
        testFieldEqual<pvd::PVInt>(mon.root, "y", 2);
        testEqual(mon.changed, pvd::BitSet().set(1));
        testEqual(mon.overrun, pvd::BitSet());

        testOk1(mon.poll());
        testFieldEqual<pvd::PVInt>(mon.root, "x", 51);
        testFieldEqual<pvd::PVInt>(mon.root, "y", 2);
        testEqual(mon.changed, pvd::BitSet().set(1));
        testEqual(mon.overrun, pvd::BitSet());

        testOk1(mon.poll());
        testFieldEqual<pvd::PVInt>(mon.root, "x", 53);
        testFieldEqual<pvd::PVInt>(mon.root, "y", 2);
        testEqual(mon.changed, pvd::BitSet().set(1));
        testEqual(mon.overrun, pvd::BitSet().set(1));

        testOk1(!mon.poll());

        testOk1(!mon.wait(0.1)); // timeout
        testOk1(!mon.poll());
    }
};
} // namespace

MAIN(testmon)
{
    testPlan(63);
    TEST_METHOD(TestMonitor, test_event);
    TEST_METHOD(TestMonitor, test_share);
    TEST_METHOD(TestMonitor, test_overflow_downstream);
    int ok = 1;
    size_t temp;
#define TESTC(name) temp=epicsAtomicGetSizeT(&name::num_instances); ok &= temp==0; testDiag("num. live "  #name " %u", (unsigned)temp)
    TESTC(GWChannel);
    TESTC(ChannelCacheEntry::CRequester);
    TESTC(ChannelCacheEntry);
    TESTC(MonitorCacheEntry);
    TESTC(MonitorUser);
#undef TESTC
    testOk(ok, "All instances free'd");
    return testDone();
}
