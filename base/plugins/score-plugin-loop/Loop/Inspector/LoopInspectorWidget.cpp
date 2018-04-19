// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopInspectorWidget.hpp"

#include <Inspector/InspectorWidgetList.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/ProcessList.hpp>
#include <QVBoxLayout>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorWidget.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
LoopInspectorWidget::LoopInspectorWidget(
    const Loop::ProcessModel& object,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
{
  // FIXME URGENT add implemented virtual destructors to every class that
  // inherits from a virtual.
  auto& appContext = doc.app;
  auto& intervalWidgetFactory
      = appContext
            .interfaces<Scenario::IntervalInspectorDelegateFactoryList>();

  auto& interval = object.interval();

  auto delegate = intervalWidgetFactory.make(
      &Scenario::IntervalInspectorDelegateFactory::make, interval);
  if (!delegate)
    return;

  auto lay = new QVBoxLayout;
  this->setLayout(lay);
  auto& widgetFact = appContext.interfaces<Inspector::InspectorWidgetList>();
  lay->addWidget(new Scenario::IntervalInspectorWidget{
      widgetFact, interval, std::move(delegate), doc, this});
}
