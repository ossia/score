#pragma once
#include <Scenario/Document/State/StateView.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <score/model/Identifier.hpp>
#include <score/widgets/GraphicsItem.hpp>
#include <score_plugin_scenario_export.h>

namespace Scenario
{
class StateModel;
class SCORE_PLUGIN_SCENARIO_EXPORT StatePresenter final : public QObject
{
  Q_OBJECT

public:
  StatePresenter(
      const StateModel& model, QGraphicsItem* parentview, QObject* parent);

  virtual ~StatePresenter();

  const Id<StateModel>& id() const;

  StateView* view() const;

  const StateModel& model() const;

  bool isSelected() const;

  void handleDrop(const QMimeData* mime);

Q_SIGNALS:
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
