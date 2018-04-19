#pragma once
#include <Loop/LoopProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <QVBoxLayout>

class QWidget;
namespace score
{
struct DocumentContext;
} // namespace score

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
      const score::DocumentContext& context,
      QWidget* parent);
};
