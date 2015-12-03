#pragma once
#include <iscore/serialization/VisitorInterface.hpp>

class ConstraintModel;
class LayerModel;
class QObject;


template<typename T>
LayerModel* createLayerModel(
        Deserializer<T>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent);
