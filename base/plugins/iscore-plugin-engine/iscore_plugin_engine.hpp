#pragma once
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <QObject>
#include <QStringList>
#include <vector>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

/**
 * \namespace Engine
 * \brief Link of i-score with the OSSIA API execution engine.
 *
 * This namespace provides the tools that are used when going from
 * the i-score model, with purely serializable data structures, to
 * the OSSIA API model.
 *
 * OSSIA implementations of the protocolsare also provided.
 *
 * There are three main parts :
 *
 * * \ref Engine::LocalTree handles the conversion from i-score's data structures to
 * the Local protocol.
 * * \ref Engine::Execution handles the conversion from i-score's data structures to
 * the OSSIA classes responsible for the execution (ossia::time_process, ossia::clock, etc.)
 * * \ref Engine::Network wraps the various OSSIA protocols (Minuit, OSC, etc.)
 *   behind Device::DeviceInterface, and provides edition widgets for these protocols.
 * * Classes used to handle the various node listening strategies are provided.
 *
 * Two files, \ref iscore2OSSIA.hpp and \ref OSSIA2iscore.hpp contain tools
 * to convert the various data structures of each environment into each other.
 */

/**
 * \namespace Engine::LocalTree
 * \brief Local tree provides a way to extend the tree given through the \ref Engine::Network::LocalDevice.
 *
 * It is a device tree used to access to i-score's internal data structures from the outside,
 * or use it in automations, etc.
 *
 * For instance, it allows to do conditions based on the advancement of each Scenario::ConstraintModel.
 *
 * It is possible to extend the tree for Process::ProcessModel%s, or other polymorphic types.
 */

/**
 * \namespace Engine::Execution
 * \brief Components used for the execution of a score.
 *
 * This hierarchy of types is used to create the OSSIA structures for the execution behind
 * iscore::Component%s.
 *
 * Currently, all the execution structures are recreated from scratch when
 * doing "play".
 *
 * The classes inheriting from Engine::Execution::ProcessComponent
 * are first created, then the Engine::Execution::ConstraintComponent will ask
 * them for their ossia::time_process which it gives to the matching ossia::time_constraint.
 *
 * \todo This should be updated in order to allow live edition of scores.
 *
 */

/**
 * \namespace Engine::Network
 * \brief OSSIA protocols wrapped into i-score
 *
 * This namespace provides the implementations in i-score for
 * various protocols: OSC, MIDI, Minuit, HTTP, WebSocket...
 */

class iscore_plugin_engine final :
        public QObject,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::FactoryList_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::Plugin_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID GUIApplicationContextPlugin_QtInterface_iid)
        Q_INTERFACES(
            iscore::GUIApplicationContextPlugin_QtInterface
                iscore::FactoryList_QtInterface
            iscore::FactoryInterface_QtInterface
                iscore::Plugin_QtInterface
        )

    public:
        iscore_plugin_engine();
        virtual ~iscore_plugin_engine();

    private:
        iscore::GUIApplicationContextPlugin* make_applicationPlugin(
                const iscore::GUIApplicationContext& app) override;

        std::vector<std::unique_ptr<iscore::FactoryListInterface>> factoryFamilies() override;

        // Contains the OSC, MIDI, Minuit factories
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> factories(
                const iscore::ApplicationContext&,
                const iscore::AbstractFactoryKey& factoryName) const override;

        QStringList required() const override;
        QStringList offered() const override;
        iscore::Version version() const override;
        UuidKey<iscore::Plugin> key() const override;
};

