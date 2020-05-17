#pragma once
#include <Dataflow/PortItem.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <score/document/DocumentContext.hpp>

namespace Dataflow
{

class PortTooltip final : public QWidget
{
public:
  PortTooltip(const score::DocumentContext& ctx, const Process::Inlet& port, QWidget* parent)
      : QWidget{parent}
  {
    auto lay = new Inspector::Layout{this};
    lay->addRow(port.customData(), (QWidget*)nullptr);

    auto& pf = ctx.app.interfaces<Process::PortFactoryList>();
    auto fact = pf.get(port.concreteKey());
    if (fact)
    {
      fact->setupInletInspector(port, ctx, this, *lay, this);
    }
    else
    {
      Process::PortWidgetSetup::setupAlone(port, ctx, *lay, this);
    }
  }
  PortTooltip(const score::DocumentContext& ctx, const Process::Outlet& port, QWidget* parent)
      : QWidget{parent}
  {
    auto lay = new Inspector::Layout{this};
    lay->addRow(port.customData(), (QWidget*)nullptr);

    auto& pf = ctx.app.interfaces<Process::PortFactoryList>();
    auto fact = pf.get(port.concreteKey());
    if (fact)
    {
      fact->setupOutletInspector(port, ctx, this, *lay, this);
    }
    else
    {
      Process::PortWidgetSetup::setupAlone(port, ctx, *lay, this);
    }
  }
};


class SCORE_PLUGIN_DATAFLOW_EXPORT InletInspectorFactory final
    : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("1e7166bb-278a-49ce-b6a9-d662b8cd8dd2")
public:
  InletInspectorFactory() : InspectorWidgetFactory{} { }

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override
  {
    return new PortTooltip{doc, safe_cast<const Process::Inlet&>(*sourceElements.first()), parent};
  }

  bool matches(const InspectedObjects& objects) const override
  {
    return qobject_cast<const Process::Inlet*>(objects.first());
  }
};
class SCORE_PLUGIN_DATAFLOW_EXPORT OutletInspectorFactory final
    : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("2479a7a1-8dbc-49d5-a146-3d29d5106cba")
public:
    public:
  OutletInspectorFactory() : InspectorWidgetFactory{} { }

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override
  {
    return new PortTooltip{doc, safe_cast<const Process::Outlet&>(*sourceElements.first()), parent};
  }

  bool matches(const InspectedObjects& objects) const override
  {
    return qobject_cast<const Process::Outlet*>(objects.first());
  }
};
}
