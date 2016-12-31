#include "ProcessComponent.hpp"

namespace Engine
{
namespace Execution
{
ProcessComponent::~ProcessComponent() = default;
ProcessComponentFactory::~ProcessComponentFactory() = default;

void ProcessComponentFactory::init(ProcessComponent* comp) const
{

}

}
}
