#include "SpaceApplicationPlugin.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
SpaceApplicationPlugin::SpaceApplicationPlugin(
        iscore::Presenter* pres) :
    GUIApplicationContextPlugin {pres, "SpaceApplicationPlugin", nullptr}
{
}

SpaceApplicationPlugin::~SpaceApplicationPlugin()
{

}
