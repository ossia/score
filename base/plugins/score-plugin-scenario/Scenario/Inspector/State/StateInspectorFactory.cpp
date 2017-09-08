// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <Scenario/Document/State/StateModel.hpp>

#include "StateInspectorFactory.hpp"
#include "StateInspectorWidget.hpp"

namespace Scenario
{
QWidget* StateInspectorFactory::make(
    const QList<const QObject*>& sourceElements,
    const score::DocumentContext& doc,
    QWidget* parentWidget) const
{
  auto baseW = new Inspector::InspectorWidgetBase{
      static_cast<const StateModel&>(*sourceElements.first()), doc,
      parentWidget};
  /*
  auto contentW = new StateInspectorWidget{
                  static_cast<const StateModel&>(*sourceElements.first()),
                          doc,
                          parentWidget};
*/
  //    baseW->updateAreaLayout({contentW});
  return baseW;
}

bool StateInspectorFactory::matches(const QList<const QObject*>& objects) const
{
  return dynamic_cast<const StateModel*>(objects.first());
}
}
