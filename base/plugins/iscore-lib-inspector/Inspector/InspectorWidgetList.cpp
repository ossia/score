// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <iscore/document/DocumentInterface.hpp>

#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetList.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <iscore/model/IdentifiedObjectAbstract.hpp>

class QWidget;

namespace Inspector
{
QList<QWidget*> InspectorWidgetList::make(
    const iscore::DocumentContext& doc,
    QList<const IdentifiedObjectAbstract*>
        models,
    QWidget* parent) const
{
  QList<QWidget*> widgs;
  for (const InspectorWidgetFactory& factory : *this)
  {
    // TODO QVector or better, array_view
    QList<const QObject*> objects;
    objects.reserve(models.size());
    for (auto elt : models)
    {
      objects.push_back(elt);
    }

    if (factory.matches(objects))
    {
      auto widg = factory.makeWidget(objects, doc, parent);
      if (widg)
        widgs.push_back(widg);
    }
  }

  // When no factory is found.
  if (widgs.empty())
    widgs.push_back(new InspectorWidgetBase{*models.first(), doc, nullptr});
  return widgs;
}
}
