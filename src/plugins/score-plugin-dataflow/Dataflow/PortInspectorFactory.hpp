#pragma once
#include <Dataflow/PortItem.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Dataflow/PortListWidget.hpp>

namespace Dataflow
{

class PortTooltip final : public QWidget
{
public:
  PortTooltip(const score::DocumentContext& ctx, const Process::Port& p, QWidget* parent)
      : QWidget{parent}
  {
    auto lay = new Inspector::Layout{this};
    lay->addRow(p.customData(), (QWidget*)nullptr);
    Process::PortWidgetSetup::setupAlone(p, ctx, *lay, this);
  }
};

class SCORE_PLUGIN_DATAFLOW_EXPORT PortInspectorFactory final
    : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("1e7166bb-278a-49ce-b6a9-d662b8cd8dd2")
public:
  PortInspectorFactory() : InspectorWidgetFactory{} { }

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override
  {
    return new PortTooltip{doc, safe_cast<const Process::Port&>(*sourceElements.first()), parent};
  }

  bool matches(const InspectedObjects& objects) const override
  {
    return dynamic_cast<const Process::Port*>(objects.first());
  }
};
}
