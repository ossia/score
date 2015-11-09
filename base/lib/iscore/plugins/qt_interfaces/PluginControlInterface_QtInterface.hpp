#pragma once
#include <QObject>

namespace iscore
{
class Presenter;
    class PluginControlInterface;
    class PluginControlInterface_QtInterface
    {
        public:
            virtual ~PluginControlInterface_QtInterface();

            virtual PluginControlInterface* make_control(Presenter* parent) = 0;
    };
}


#define PluginControlInterface_QtInterface_iid "org.ossia.i-score.plugins.PluginControlInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PluginControlInterface_QtInterface, PluginControlInterface_QtInterface_iid)


// TODO moveme
#include <QObject>
#include <iscore/command/CommandGeneratorMap.hpp>
namespace iscore
{
class CommandFactory_QtInterface
{
    public:
        virtual ~CommandFactory_QtInterface() = default;

        virtual std::pair<const std::string, CommandGeneratorMap> make_commands() = 0;
};
}


#define CommandFactory_QtInterface_iid "org.ossia.i-score.plugins.CommandFactory_QtInterface"

Q_DECLARE_INTERFACE(iscore::CommandFactory_QtInterface, CommandFactory_QtInterface_iid)
