// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
