#pragma once
#include <Recording/Record/RecordTools.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore_plugin_recording_export.h>

namespace Scenario
{
class ProcessModel;
}
namespace Explorer
{
class DeviceExplorerModel;
}
namespace Recording
{
using Priority = int;
struct RecordContext
{
        RecordContext(const Scenario::ProcessModel&, Scenario::Point pt);

        const iscore::DocumentContext& context;
        const Scenario::ProcessModel& scenario;
        Explorer::DeviceExplorerModel& explorer;
        RecordCommandDispatcher dispatcher;

        Scenario::Point point;
};

struct RecordProvider
{
        virtual ~RecordProvider();
        virtual void setup() = 0;
        virtual void stop() = 0;
};

class ISCORE_PLUGIN_RECORDING_EXPORT RecordProviderFactory :
        public iscore::AbstractFactory<RecordProviderFactory>
{
    ISCORE_ABSTRACT_FACTORY("64999184-a705-4686-b967-14e8f79692f1")
    public:
        virtual ~RecordProviderFactory();
        /**
         * @brief matches
         * @return <= 0 : does not match
         * > 0 : matches. The highest priority should be taken.
         */
        virtual Priority matches(
            const Device::Node&,
            const iscore::DocumentContext& ctx) = 0;

        virtual std::unique_ptr<RecordProvider> make(
            const Device::NodeList&,
            const iscore::DocumentContext& ctx) = 0;


};
}
