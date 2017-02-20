#pragma once
#include <Scenario/Document/State/StateView.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <iscore/model/Identifier.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class StateModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT StatePresenter final : public QObject
{
  Q_OBJECT

public:
  StatePresenter(
      const StateModel& model, QQuickPaintedItem* parentview, QObject* parent);

  virtual ~StatePresenter();

  const Id<StateModel>& id() const;

  StateView* view() const;

  const StateModel& model() const;

  bool isSelected() const;

  void handleDrop(const QMimeData* mime);

signals:
  void pressed(const QPointF&);
  void moved(const QPointF&);
  void released(const QPointF&);

  void hoverEnter();
  void hoverLeave();

  void askUpdate();

private:
  void updateStateView();

  const StateModel& m_model;
  graphics_item_ptr<StateView> m_view{};

  CommandDispatcher<> m_dispatcher;
};
}
