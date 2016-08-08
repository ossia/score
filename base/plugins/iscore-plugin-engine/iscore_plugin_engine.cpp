#include <Engine/Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <Engine/Protocols/OSC/OSCProtocolFactory.hpp>
#include <Engine/Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Engine/Protocols/Local/LocalProtocolFactory.hpp>

#include <Engine/Protocols/Panel/MessagesPanel.hpp>

#include <QString>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Engine/ApplicationPlugin.hpp>

#include <Engine/LocalTree/Scenario/ScenarioComponent.hpp>
#include <Engine/Executor/ProcessElement.hpp>
#include <Engine/Executor/ScenarioElement.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_engine.hpp"
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <Engine/Curve/EasingSegment.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/Settings/ExecutorFactory.hpp>
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <Engine/Executor/StateProcessComponent.hpp>
#include <Engine/Executor/ClockManager/DefaultClockManager.hpp>
#include <Engine/LocalTree/Settings/LocalTreeFactory.hpp>
#include <Engine/Listening/PlayListeningHandlerFactory.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
iscore_plugin_engine::iscore_plugin_engine() :
    QObject {}
{
    qRegisterMetaType<Engine::Execution::ClockManagerFactory::ConcreteFactoryKey>("ClockManagerKey");
    qRegisterMetaTypeStreamOperators<Engine::Execution::ClockManagerFactory::ConcreteFactoryKey>("ClockManagerKey");
}

iscore_plugin_engine::~iscore_plugin_engine()
{

}

iscore::GUIApplicationContextPlugin* iscore_plugin_engine::make_applicationPlugin(
        const iscore::GUIApplicationContext& app)
{
    return new ApplicationPlugin{app};
}

std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_engine::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Engine::LocalTree::ProcessComponentFactoryList,
            Engine::Execution::ProcessComponentFactoryList,
            Engine::Execution::StateProcessComponentFactoryList,
            Engine::Execution::ClockManagerFactoryList
            >();
}



std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_engine::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    using namespace Scenario;
    using namespace Engine;
    using namespace Engine::Execution;
    using namespace EasingCurve;

    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
            FW<Device::ProtocolFactory,
                 Network::LocalProtocolFactory,
                 Network::OSCProtocolFactory,
                 Network::MinuitProtocolFactory,
                 Network::MIDIProtocolFactory
            >,
            FW<Engine::Execution::ProcessComponentFactory,
                 Engine::Execution::ScenarioComponentFactory>,
            FW<Explorer::ListeningHandlerFactory,
                 Engine::Execution::PlayListeningHandlerFactory>,
            FW<iscore::SettingsDelegateFactory,
                 Engine::Execution::Settings::Factory,
                 LocalTree::Settings::Factory>,
            FW<Engine::LocalTree::ProcessComponentFactory,
                 Engine::LocalTree::ScenarioComponentFactory>,
            FW<iscore::PanelDelegateFactory,
                 Engine::PanelDelegateFactory>,
            FW<Engine::Execution::ClockManagerFactory, Engine::Execution::DefaultClockManagerFactory>,
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
            >>
               >(ctx, key);
}


QStringList iscore_plugin_engine::required() const
{
    return {"Scenario", "DeviceExplorer"};
}

QStringList iscore_plugin_engine::offered() const
{
    return {"Engine"};
}

iscore::Version iscore_plugin_engine::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_engine::key() const
{
    return_uuid("d4758f8d-64ac-41b4-8aaf-1cbd6f3feb91");
}
