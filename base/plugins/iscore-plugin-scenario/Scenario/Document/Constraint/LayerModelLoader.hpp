#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
namespace Scenario
{
class ConstraintModel;
}
namespace Process
{
class LayerModel;
class ProcessList;
}
class QObject;

// TODO change namespace to Scenario
namespace Process
{
template<typename T>
LayerModel* createLayerModel(
        const Process::ProcessList& processes,
        Deserializer<T>& deserializer,
        const Scenario::ConstraintModel& constraint,
        QObject* parent);
}
