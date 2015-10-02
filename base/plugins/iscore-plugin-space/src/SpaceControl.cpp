#include "SpaceControl.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
SpaceControl::SpaceControl(
        iscore::Presenter* pres) :
    PluginControlInterface {pres, "SpaceControl", nullptr}
{
    setupCommands();
}

namespace {
struct SpaceCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap SpaceCommandFactory::map;
}

void SpaceControl::setupCommands()
{
    boost::mpl::for_each<
            boost::mpl::list<
            >,
            boost::type<boost::mpl::_>
    >(CommandGeneratorMapInserter<SpaceCommandFactory>());
}

iscore::SerializableCommand* SpaceControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<SpaceCommandFactory>(name, data);
}
