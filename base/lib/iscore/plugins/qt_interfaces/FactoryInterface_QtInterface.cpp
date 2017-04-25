#include "FactoryInterface_QtInterface.hpp"
namespace iscore
{
FactoryInterface_QtInterface::~FactoryInterface_QtInterface()
= default;

std::vector<std::unique_ptr<InterfaceBase> >
FactoryInterface_QtInterface::factories(
    const ApplicationContext& ctx,
    const InterfaceKey& key) const
{ return {}; }

std::vector<std::unique_ptr<InterfaceBase> >
FactoryInterface_QtInterface::factories(
    const GUIApplicationContext& ctx,
    const InterfaceKey& key) const
{ return {}; }

}
