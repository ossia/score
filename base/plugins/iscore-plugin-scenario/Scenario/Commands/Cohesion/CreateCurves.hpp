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
class IntervalModel;
void CreateCurves(
    const QList<const Scenario::IntervalModel*>& selected_intervals,
    const iscore::CommandStackFacade& stack);
void CreateCurvesFromAddresses(
    const QList<const Scenario::IntervalModel*>& selected_intervals,
    const std::vector<Device::FullAddressSettings>& addresses,
    const iscore::CommandStackFacade& stack);
}
