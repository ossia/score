#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
class QObject;
class ProcessViewModel;
class ProcessModel;
class ConstraintModel;


template<typename T>
ProcessViewModel* createProcessViewModel(Deserializer<T>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent);
