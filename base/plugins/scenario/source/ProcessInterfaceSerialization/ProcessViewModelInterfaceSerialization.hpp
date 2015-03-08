#pragma once
#include <public_interface/serialization/VisitorInterface.hpp>
class QObject;
class ProcessViewModelInterface;
class ProcessSharedModelInterface;
class ConstraintModel;


template<typename T>
ProcessViewModelInterface* createProcessViewModel(Deserializer<T>& deserializer,
        ConstraintModel* constraint,
        QObject* parent);
