#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <QVBoxLayout>

class QWidget;
namespace iscore
{
struct DocumentContext;
} // namespace iscore

namespace Loop
{
class ProcessModel;
}

class LoopInspectorWidget final
    : public Process::InspectorWidgetDelegate_T<Loop::ProcessModel>
{
public:
  explicit LoopInspectorWidget(
      const Loop::ProcessModel& object,
      const iscore::DocumentContext& context,
      QWidget* parent);
};
