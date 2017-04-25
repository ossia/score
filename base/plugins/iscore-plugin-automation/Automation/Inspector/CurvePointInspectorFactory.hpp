#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

namespace Automation
{
class PointInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  ISCORE_CONCRETE("c2fc4c5b-641f-41e3-8734-5caf77b27de8")
public:
  PointInspectorFactory() : InspectorWidgetFactory{}
  {
  }

  Inspector::InspectorWidgetBase* makeWidget(
      const QList<const QObject*>& sourceElements,
      const iscore::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const QList<const QObject*>& objects) const override;
};
}
