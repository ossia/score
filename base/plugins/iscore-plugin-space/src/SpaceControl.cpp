#include "SpaceControl.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
SpaceControl::SpaceControl(
        iscore::Presenter* pres) :
    GUIApplicationContextPlugin {pres, "SpaceControl", nullptr}
{
}

SpaceControl::~SpaceControl()
{

}
