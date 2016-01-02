#pragma once
#include <iscore/serialization/VisitorInterface.hpp>

class ConstraintModel;
namespace Process { class LayerModel; }
class QObject;

namespace Process
{
template<typename T>
LayerModel* createLayerModel(
        Deserializer<T>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent);
}
