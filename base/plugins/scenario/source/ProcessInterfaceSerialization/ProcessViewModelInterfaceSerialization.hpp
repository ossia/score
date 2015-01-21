#pragma once
#include <interface/serialization/VisitorInterface.hpp>
class QObject;
class ProcessViewModelInterface;
class ConstraintModel;

template<typename T>
ProcessViewModelInterface* createProcessViewModel(Deserializer<T>& deserializer,
												  ConstraintModel* constraint,
												  QObject* parent);
