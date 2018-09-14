// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopInspectorWidget.hpp"

#include <Inspector/InspectorWidgetList.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorWidget.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <QVBoxLayout>
namespace Loop
{
InspectorWidget::InspectorWidget(
    const Loop::ProcessModel& object, const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
{
  // FIXME URGENT add implemented virtual destructors to every class that
  // inherits from a virtual.
  auto& appContext = doc.app;

  auto& interval = object.interval();

  auto lay = new QVBoxLayout;
  this->setLayout(lay);
  auto& widgetFact = appContext.interfaces<Inspector::InspectorWidgetList>();
  lay->addWidget(
      new Scenario::IntervalInspectorWidget{widgetFact, interval, doc, this});
}

InspectorWidget::~InspectorWidget()
{
}
}
