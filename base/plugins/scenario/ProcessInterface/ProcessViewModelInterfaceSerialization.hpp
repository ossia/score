#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>
class ProcessViewModelInterface;
class ConstraintModel;
ProcessViewModelInterface* createProcessViewModel(Deserializer<DataStream>& deserializer,
												  ConstraintModel* constraint,
												  QObject* parent);
