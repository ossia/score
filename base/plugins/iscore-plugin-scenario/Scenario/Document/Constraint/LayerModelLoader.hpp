#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
class QObject;
class LayerModel;
class Process;
class ConstraintModel;


template<typename T>
LayerModel* createLayerModel(
        Deserializer<T>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent);
