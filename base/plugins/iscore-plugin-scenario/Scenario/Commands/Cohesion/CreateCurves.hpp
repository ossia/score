#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <QList>
#include <vector>
namespace iscore
{
class CommandStackFacade;
struct DocumentContext;
}
namespace Scenario
{
class ConstraintModel;
void CreateCurves(
    const QList<const Scenario::ConstraintModel*>& selected_constraints,
    const iscore::CommandStackFacade& stack);
void CreateCurvesFromAddresses(
    const QList<const Scenario::ConstraintModel*>& selected_constraints,
    const std::vector<Device::FullAddressSettings>& addresses,
    const iscore::CommandStackFacade& stack);
}
