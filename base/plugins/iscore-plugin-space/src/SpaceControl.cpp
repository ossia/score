#include "SpaceControl.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
SpaceControl::SpaceControl(
        iscore::Presenter* pres) :
    PluginControlInterface {pres, "SpaceControl", nullptr}
{
}

SpaceControl::~SpaceControl()
{

}
