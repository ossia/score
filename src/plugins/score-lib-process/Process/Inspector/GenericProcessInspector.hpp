#pragma once
#include <Inspector/InspectorLayout.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
namespace Process
{

template <typename P>
class GenericInspectorWidget : public Process::InspectorWidgetDelegate_T<P>
{
public:
  explicit GenericInspectorWidget(
      const P& object,
      const score::DocumentContext& doc,
      QWidget* parent)
      : InspectorWidgetDelegate_T<P>{object, parent}
  {
    auto lay = new score::MarginLess<QVBoxLayout>{this};
    lay->addWidget(new PortListWidget{object, doc, this});
  }

  ~GenericInspectorWidget() = default;
};
}
