#pragma once
#include <QObject>

class QQuickPaintedItem;

namespace Scenario
{
class TriggerModel;
class TriggerView;

class TriggerPresenter final : public QObject
{
  Q_OBJECT
public:
  TriggerPresenter(
      const TriggerModel&, QQuickPaintedItem*, QObject* parent = nullptr);

  const TriggerModel& model() const
  {
    return m_model;
  }

private:
  const TriggerModel& m_model;
  TriggerView* m_view{};
};
}
