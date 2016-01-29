#ifndef UTILITIES_H
#define UTILITIES_H

#include <deque>

#include <epicsEvent.h>
#include <epicsUnitTest.h>

#include <pv/pvAccess.h>

#include "weakmap.h"
#include "weakset.h"

struct TestPV;
struct TestPVChannel;
struct TestPVMonitor;
struct TestProvider;

// minimally useful boilerplate which must appear *everywhere*
#define DUMBREQUESTER(NAME) \
    virtual std::string getRequesterName() { return #NAME; } \
    virtual void message(std::string const & message,epics::pvData::MessageType messageType) { \
        testDiag("%s : " #NAME "(%p) : %s", epics::pvData::getMessageTypeName(messageType).c_str(), this, message.c_str()); \
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
};

struct TestChannelRequester : public epics::pvAccess::ChannelRequester
{
    POINTER_DEFINITIONS(TestChannelRequester);
    DUMBREQUESTER(TestChannelRequester)

    epicsMutex lock;
    epicsEvent wait;
    epics::pvAccess::Channel::shared_pointer chan;
    epics::pvData::Status status;
    epics::pvAccess::Channel::ConnectionState laststate;
    TestChannelRequester();
    virtual ~TestChannelRequester() {}
    virtual void channelCreated(const epics::pvData::Status& status, epics::pvAccess::Channel::shared_pointer const & channel);
    virtual void channelStateChange(epics::pvAccess::Channel::shared_pointer const & channel, epics::pvAccess::Channel::ConnectionState connectionState);

    bool waitForConnect();
};

struct TestChannelMonitorRequester : public epics::pvData::MonitorRequester
{
    POINTER_DEFINITIONS(TestChannelMonitorRequester);
    DUMBREQUESTER(TestChannelMonitorRequester)

    epicsMutex lock;
    epicsEvent wait;
    bool connected;
    bool unlistend;
    size_t eventCnt;
    epics::pvData::Status connectStatus;
    epics::pvData::MonitorPtr mon;
    epics::pvData::StructureConstPtr dtype;

    TestChannelMonitorRequester();
    virtual ~TestChannelMonitorRequester() {}

    virtual void monitorConnect(epics::pvData::Status const & status,
                                epics::pvData::MonitorPtr const & monitor,
                                epics::pvData::StructureConstPtr const & structure);
    virtual void monitorEvent(epics::pvData::MonitorPtr const & monitor);
    virtual void unlisten(epics::pvData::MonitorPtr const & monitor);

    bool waitForEvent();
};

struct TestPVChannel : public epics::pvAccess::Channel
{
    POINTER_DEFINITIONS(TestPVChannel);
    DUMBREQUESTER(TestPVChannel)
    std::tr1::weak_ptr<TestPVChannel> weakself;

    const std::tr1::shared_ptr<TestPV> pv;
    std::tr1::shared_ptr<epics::pvAccess::ChannelRequester> requester;
    ConnectionState state;

    typedef weak_set<TestPVMonitor> monitors_t;
    monitors_t monitors;

    TestPVChannel(const std::tr1::shared_ptr<TestPV>& pv,
                  const std::tr1::shared_ptr<epics::pvAccess::ChannelRequester>& req);
    virtual ~TestPVChannel();

    virtual void destroy();

    virtual std::tr1::shared_ptr<epics::pvAccess::ChannelProvider> getProvider();
    virtual std::string getRemoteAddress() { return "localhost:1234"; }
    virtual ConnectionState getConnectionState();
    virtual std::string getChannelName();
    virtual std::tr1::shared_ptr<epics::pvAccess::ChannelRequester> getChannelRequester()
    { return std::tr1::shared_ptr<epics::pvAccess::ChannelRequester>(requester); }
    virtual bool isConnected();
    virtual void getField(epics::pvAccess::GetFieldRequester::shared_pointer const & requester,std::string const & subField);
    virtual epics::pvAccess::AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField)
    { return epics::pvAccess::readWrite; }

    virtual epics::pvAccess::ChannelProcess::shared_pointer createChannelProcess(
            epics::pvAccess::ChannelProcessRequester::shared_pointer const & channelProcessRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual epics::pvAccess::ChannelGet::shared_pointer createChannelGet(
            epics::pvAccess::ChannelGetRequester::shared_pointer const & channelGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual epics::pvAccess::ChannelPut::shared_pointer createChannelPut(
            epics::pvAccess::ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual epics::pvAccess::ChannelPutGet::shared_pointer createChannelPutGet(
            epics::pvAccess::ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual epics::pvAccess::ChannelRPC::shared_pointer createChannelRPC(
            epics::pvAccess::ChannelRPCRequester::shared_pointer const & channelRPCRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual epics::pvData::Monitor::shared_pointer createMonitor(
            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual epics::pvAccess::ChannelArray::shared_pointer createChannelArray(
            epics::pvAccess::ChannelArrayRequester::shared_pointer const & channelArrayRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual void printInfo() {}
    virtual void printInfo(std::ostream& out) {}
};

struct TestPVMonitor : public epics::pvData::Monitor
{
    POINTER_DEFINITIONS(TestPVMonitor);
    std::tr1::weak_ptr<TestPVMonitor> weakself;

    TestPVChannel::shared_pointer channel;
    epics::pvData::MonitorRequester::shared_pointer requester;

    bool running;
    bool finalize;
    bool inoverflow;
    bool needWakeup;

    TestPVMonitor(const TestPVChannel::shared_pointer& ch,
                  const epics::pvData::MonitorRequester::shared_pointer& req,
                  size_t bsize);
    virtual ~TestPVMonitor();

    virtual void destroy();

    virtual epics::pvData::Status start();
    virtual epics::pvData::Status stop();
    virtual epics::pvData::MonitorElementPtr poll();
    virtual void release(epics::pvData::MonitorElementPtr const & monitorElement);

    std::deque<epics::pvData::MonitorElementPtr> buffer, free;
    epics::pvData::BitSet changedMask, overflowMask;
};

struct TestPV
{
    POINTER_DEFINITIONS(TestPV);
    std::tr1::weak_ptr<TestPV> weakself;

    const std::string name;
    std::tr1::shared_ptr<TestProvider> const provider;
    epics::pvData::PVDataCreatePtr factory;
    const epics::pvData::StructureConstPtr dtype;
    epics::pvData::PVStructurePtr value;

    TestPV(const std::string& name,
           const std::tr1::shared_ptr<TestProvider>& provider,
           const epics::pvData::StructureConstPtr& dtype);

    void post(const epics::pvData::BitSet& changed, bool notify = true);

    void disconnect();

    typedef weak_set<TestPVChannel> channels_t;
    channels_t channels;
    friend struct TestProvider;
};

struct TestProvider : public epics::pvAccess::ChannelProvider, std::tr1::enable_shared_from_this<TestProvider>
{
    POINTER_DEFINITIONS(TestProvider);

    virtual std::string getProviderName() { return "TestProvider"; }

    virtual void destroy();

    virtual epics::pvAccess::ChannelFind::shared_pointer channelFind(std::string const & channelName,
                                             epics::pvAccess::ChannelFindRequester::shared_pointer const & channelFindRequester);
    virtual epics::pvAccess::ChannelFind::shared_pointer channelList(epics::pvAccess::ChannelListRequester::shared_pointer const & channelListRequester);
    virtual epics::pvAccess::Channel::shared_pointer createChannel(std::string const & channelName,epics::pvAccess::ChannelRequester::shared_pointer const & channelRequester,
                                           short priority = PRIORITY_DEFAULT);
    virtual epics::pvAccess::Channel::shared_pointer createChannel(std::string const & channelName, epics::pvAccess::ChannelRequester::shared_pointer const & channelRequester,
                                           short priority, std::string const & address);

    TestProvider();
    virtual ~TestProvider() {}

    TestPV::shared_pointer addPV(const std::string& name, const epics::pvData::StructureConstPtr& tdef);

    void dispatch();

    epicsMutex lock;
    typedef weak_value_map<std::string, TestPV> pvs_t;
    pvs_t pvs;
};

#endif // UTILITIES_H