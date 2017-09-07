// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Loop/LoopProcessModel.hpp>

#include "LoopInspectorWidget.hpp"
#include <QVBoxLayout>

#include <Inspector/InspectorWidgetList.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorWidget.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
LoopInspectorWidget::LoopInspectorWidget(
    const Loop::ProcessModel& object,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
{
  // FIXME URGENT add implemented virtual destructors to every class that
  // inherits from a virtual.
  auto& appContext = doc.app;
  auto& intervalWidgetFactory
      = appContext.interfaces<Scenario::IntervalInspectorDelegateFactoryList>();

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
