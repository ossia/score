#pragma once
#include <Scenario/Document/State/StateView.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/Identifier.hpp>

#include <score_plugin_scenario_export.h>

#include <verdigris>

namespace Scenario
{
class StateModel;
class SCORE_PLUGIN_SCENARIO_EXPORT StatePresenter final : public QObject
{
  W_OBJECT(StatePresenter)

public:
  StatePresenter(
      const StateModel& model,
      const score::DocumentContext& ctx,
      QGraphicsItem* parentview,
      QObject* parent);

  virtual ~StatePresenter();

  const Id<StateModel>& id() const;

  StateView* view() const;

  const StateModel& model() const;

  void select() const;
  bool isSelected() const;

  void handleDrop(const QMimeData& mime);

public:
  void pressed(const QPointF& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(const QPointF& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(const QPointF& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)

  void hoverEnter() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, hoverEnter)
  void hoverLeave() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, hoverLeave)

private:
  void updateStateView();

  const StateModel& m_model;
  graphics_item_ptr<StateView> m_view{};

  const score::DocumentContext& m_ctx;
};
}
