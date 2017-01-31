#include "ProcessComponent.hpp"
#include <Engine/Executor/ExecutorContext.hpp>
namespace Engine
{
namespace Execution
{
ProcessComponent::~ProcessComponent() = default;
ProcessComponentFactory::~ProcessComponentFactory() = default;
ProcessComponentFactoryList::~ProcessComponentFactoryList() = default;


void ProcessComponentFactory::init(ProcessComponent* comp) const
{

}

}
}
