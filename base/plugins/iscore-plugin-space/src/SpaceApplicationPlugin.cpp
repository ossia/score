#include "SpaceApplicationPlugin.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
SpaceApplicationPlugin::SpaceApplicationPlugin(
        const iscore::ApplicationContext& pres) :
    GUIApplicationContextPlugin {pres, "SpaceApplicationPlugin", nullptr}
{
}

SpaceApplicationPlugin::~SpaceApplicationPlugin()
{

}
