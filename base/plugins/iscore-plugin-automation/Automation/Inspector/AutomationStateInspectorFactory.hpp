#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

namespace Automation
{
class StateInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  ISCORE_CONCRETE("71a5f5b6-6c10-4057-ab10-278c3f18e9af")
public:
  StateInspectorFactory();

  QWidget* makeWidget(
      const QList<const QObject*>& sourceElements,
      const iscore::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const QList<const QObject*>& objects) const override;
};
}
