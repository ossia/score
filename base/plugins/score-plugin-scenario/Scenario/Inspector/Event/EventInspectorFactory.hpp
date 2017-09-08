#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class QObject;
class QString;
class QWidget;
namespace Scenario
{
class EventInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("f71c6643-cf85-4e35-a76a-b1d365416f33")
public:
  EventInspectorFactory() : InspectorWidgetFactory{}
  {
  }

  QWidget* make(
      const QList<const QObject*>& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const QList<const QObject*>& objects) const override;
};
}
