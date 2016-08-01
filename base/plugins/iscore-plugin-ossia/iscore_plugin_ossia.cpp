#include <OSSIA/Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <OSSIA/Protocols/OSC/OSCProtocolFactory.hpp>
#include <OSSIA/Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <OSSIA/Protocols/Local/LocalProtocolFactory.hpp>
#include <OSSIA/Protocols/Panel/MessagesPanel.hpp>
#include <QString>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>

#include <OSSIA/LocalTree/Scenario/ScenarioComponent.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ScenarioElement.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_ossia.hpp"
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <OSSIA/Executor/Settings/ExecutorFactory.hpp>
#include <OSSIA/Executor/ClockManager/ClockManagerFactory.hpp>
#include <OSSIA/Executor/StateProcessComponent.hpp>
#include <OSSIA/Executor/ClockManager/DefaultClockManager.hpp>
#include <OSSIA/LocalTree/Settings/LocalTreeFactory.hpp>
#include <OSSIA/Listening/PlayListeningHandlerFactory.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <OSSIA/Curve/EasingSegment.hpp>
namespace iscore {

}  // namespace iscore

iscore_plugin_ossia::iscore_plugin_ossia() :
    QObject {}
{
    qRegisterMetaType<RecreateOnPlay::ClockManagerFactory::ConcreteFactoryKey>("ClockManagerKey");
    qRegisterMetaTypeStreamOperators<RecreateOnPlay::ClockManagerFactory::ConcreteFactoryKey>("ClockManagerKey");
}

iscore_plugin_ossia::~iscore_plugin_ossia()
{

}

iscore::GUIApplicationContextPlugin* iscore_plugin_ossia::make_applicationPlugin(
        const iscore::GUIApplicationContext& app)
{
    return new OSSIAApplicationPlugin{app};
}

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_ossia::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Ossia::LocalTree::ProcessComponentFactoryList,
            RecreateOnPlay::ProcessComponentFactoryList,
            RecreateOnPlay::StateProcessComponentFactoryList,
            RecreateOnPlay::ClockManagerFactoryList
            >();
}



std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_ossia::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    using namespace Scenario;
    using namespace Ossia;
    using namespace Ossia::EasingCurve;
    using namespace RecreateOnPlay;

    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
            FW<Device::ProtocolFactory,
                 LocalProtocolFactory,
                 OSCProtocolFactory,
                 MinuitProtocolFactory,
                 MIDIProtocolFactory>,
            FW<RecreateOnPlay::ProcessComponentFactory,
                 RecreateOnPlay::ScenarioComponentFactory>,
            FW<Explorer::ListeningHandlerFactory,
                 Ossia::PlayListeningHandlerFactory>,
            FW<iscore::SettingsDelegateFactory,
                 RecreateOnPlay::Settings::Factory,
                 LocalTree::Settings::Factory>,
            FW<Ossia::LocalTree::ProcessComponentFactory,
                 Ossia::LocalTree::ScenarioComponentFactory>,
            FW<iscore::PanelDelegateFactory,
                 Ossia::PanelDelegateFactory>,
            FW<RecreateOnPlay::ClockManagerFactory,
                 RecreateOnPlay::DefaultClockManagerFactory>,
            FW<Curve::SegmentFactory,
            SegmentFactory_backIn,
            SegmentFactory_backOut,
            SegmentFactory_backInOut,
            SegmentFactory_bounceIn,
            SegmentFactory_bounceOut,
            SegmentFactory_bounceInOut,
            SegmentFactory_quadraticIn,
            SegmentFactory_quadraticOut,
            SegmentFactory_quadraticInOut,
            SegmentFactory_cubicIn,
            SegmentFactory_cubicOut,
            SegmentFactory_cubicInOut,
            SegmentFactory_quarticIn,
            SegmentFactory_quarticOut,
            SegmentFactory_quarticInOut,
            SegmentFactory_quinticIn,
            SegmentFactory_quinticOut,
            SegmentFactory_quinticInOut,
            SegmentFactory_sineIn,
            SegmentFactory_sineOut,
            SegmentFactory_sineInOut,
            SegmentFactory_circularIn,
            SegmentFactory_circularOut,
            SegmentFactory_circularInOut,
            SegmentFactory_exponentialIn,
            SegmentFactory_exponentialOut,
            SegmentFactory_exponentialInOut,
            SegmentFactory_elasticIn,
            SegmentFactory_elasticOut,
            SegmentFactory_elasticInOut
            >
            >
            >(ctx, key);
}


QStringList iscore_plugin_ossia::required() const
{
    return {"Scenario", "DeviceExplorer"};
}

QStringList iscore_plugin_ossia::offered() const
{
    return {"OSSIA"};
}

iscore::Version iscore_plugin_ossia::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_ossia::key() const
{
    return_uuid("d4758f8d-64ac-41b4-8aaf-1cbd6f3feb91");
}
