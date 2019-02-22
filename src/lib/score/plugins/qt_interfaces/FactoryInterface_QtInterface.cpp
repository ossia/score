// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "FactoryInterface_QtInterface.hpp"
namespace score
{
FactoryInterface_QtInterface::~FactoryInterface_QtInterface() = default;

std::vector<std::unique_ptr<InterfaceBase>>
FactoryInterface_QtInterface::factories(
    const ApplicationContext& ctx, const InterfaceKey& key) const
{
  return {};
}

std::vector<std::unique_ptr<InterfaceBase>>
FactoryInterface_QtInterface::guiFactories(
    const GUIApplicationContext& ctx, const InterfaceKey& key) const
{
  return {};
}
}
