#pragma once
#include <Scenario/Document/State/StateView.hpp>
#include <wobjectdefs.h>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/Identifier.hpp>
#include <score/widgets/GraphicsItem.hpp>
#include <score_plugin_scenario_export.h>

namespace Scenario
{
class StateModel;
class SCORE_PLUGIN_SCENARIO_EXPORT StatePresenter final : public QObject
{
  W_OBJECT(StatePresenter)

public:
  StatePresenter(
      const StateModel& model, QGraphicsItem* parentview, QObject* parent);

  virtual ~StatePresenter();

  const Id<StateModel>& id() const;

  StateView* view() const;

  const StateModel& model() const;

  bool isSelected() const;

  void handleDrop(const QMimeData& mime);

public:
  void pressed(const QPointF& arg_1) W_SIGNAL(pressed, arg_1);
  void moved(const QPointF& arg_1) W_SIGNAL(moved, arg_1);
  void released(const QPointF& arg_1) W_SIGNAL(released, arg_1);

  void hoverEnter() W_SIGNAL(hoverEnter);
  void hoverLeave() W_SIGNAL(hoverLeave);

  void askUpdate() W_SIGNAL(askUpdate);

private:
  void updateStateView();

  const StateModel& m_model;
  graphics_item_ptr<StateView> m_view{};

  CommandDispatcher<> m_dispatcher;
};
}
