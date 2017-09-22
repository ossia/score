#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <QList>
#include <vector>
namespace score
{
class CommandStackFacade;
struct DocumentContext;
}
namespace Scenario
{
class IntervalModel;
void CreateCurves(
    const QList<const Scenario::IntervalModel*>& selected_intervals,
    const score::CommandStackFacade& stack);
void CreateCurvesFromAddresses(
    const QList<const Scenario::IntervalModel*>& selected_intervals,
    const std::vector<Device::FullAddressSettings>& addresses,
    const score::CommandStackFacade& stack);
}
