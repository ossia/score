#pragma once
#include <QObject>

#include <iscore/application/ApplicationContext.hpp>

namespace iscore
{
    class Application;
    class GUIApplicationContextPlugin;

    class GUIApplicationContextPlugin_QtInterface
    {
        public:
            virtual ~GUIApplicationContextPlugin_QtInterface();

            virtual GUIApplicationContextPlugin* make_applicationPlugin(const iscore::ApplicationContext& app) = 0;
    };
}


#define GUIApplicationContextPlugin_QtInterface_iid "org.ossia.i-score.plugins.GUIApplicationContextPlugin_QtInterface"

Q_DECLARE_INTERFACE(iscore::GUIApplicationContextPlugin_QtInterface, GUIApplicationContextPlugin_QtInterface_iid)


#include <iscore/command/CommandGeneratorMap.hpp>
#include <utility>

#include <iscore/command/SerializableCommand.hpp>

namespace iscore
{
class CommandFactory_QtInterface
{
    public:
        virtual ~CommandFactory_QtInterface();

        virtual std::pair<const CommandParentFactoryKey, CommandGeneratorMap> make_commands() = 0;
};
}


#define CommandFactory_QtInterface_iid "org.ossia.i-score.plugins.CommandFactory_QtInterface"

Q_DECLARE_INTERFACE(iscore::CommandFactory_QtInterface, CommandFactory_QtInterface_iid)
