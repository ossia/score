#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QString>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <vector>

#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score/tools/std/OwningVector.hpp>
#include <score/tools/std/Pointer.hpp>
class IdentifiedObjectAbstract;
class QWidget;

namespace Inspector
{
class InspectorWidgetBase;
class SCORE_LIB_INSPECTOR_EXPORT InspectorWidgetList final
    : public score::MatchingFactory<InspectorWidgetFactory>
{
public:
  ~InspectorWidgetList();
  bool update(
      QWidget* cur,
      const QList<const IdentifiedObjectAbstract*>& models) const;
  QList<QWidget*> make(
      const score::DocumentContext& doc,
      const QList<const IdentifiedObjectAbstract*>& models,
      QWidget* parent) const;
};
}
