#pragma once
#include <Process/LayerView.hpp>

#include <Media/Step/Commands.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/tools/BarsDrag.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <optional>

#include <verdigris>

namespace Process
{
struct Context;
}
class QMenu;
namespace score
{
class CommandStackFacade;
}
namespace Media::Step
{
class Model;

/**
 * @brief The mouse on the bars of the sequence shown, in the layer and in the
 * node.
 *
 * As in the parameter bars of the pattern sequencer: a press sets the bar
 * under the mouse and the drag every bar it goes over; Shift-drag moves every
 * bar by the same amount; Alt-drag blends every bar towards a random value,
 * the more the further the mouse goes.
 */
class StepsGesture
{
public:
  //! The steps of the current sequence once edited, or nothing when the
  //! gesture leaves them as they are.
  std::optional<ossia::float_vector> press(
      const Model& m, int step, QPointF pos, Qt::KeyboardModifiers mods, double height);
  std::optional<ossia::float_vector>
  move(const Model& m, int step, QPointF pos, double height);
  void release() noexcept;

private:
  score::BarsDrag m_drag;
  bool m_setting{};
};

//! The current sequence, every step at a random value: one command.
void randomizeSteps(const Model& m, const score::CommandStackFacade& stack);
//! The step sequencer's entries in a context menu.
void fillStepsMenu(QMenu& menu, const Model& m, const score::CommandStackFacade& stack);

class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(const Model&, QGraphicsItem* parent);
  ~View();

  void setBarWidth(double v);

public:
  //! The steps of the current sequence while the mouse edits them; released()
  //! ends the edit.
  void stepsEdited(const ossia::float_vector& steps) W_SIGNAL(stepsEdited, steps);

private:
  void paint_impl(QPainter* p) const override;

  //! The step drawn at `x`, or -1 when the steps are too narrow to edit.
  int stepAt(double x) const noexcept;
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override;
  bool sceneEvent(QEvent* ev) override;
  void endGesture();

private:
  const Model& m_model;
  StepsGesture m_gesture;
  bool m_editing{};
  double m_barWidth{};
  //! The step that played last, -1 when stopped.
  int m_playing{-1};
};

class Item final : public score::EmptyRectItem
{
  W_OBJECT(Item)
public:
  explicit Item(const Model&, const Process::Context& ctx, QGraphicsItem* parent);
  ~Item();

private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      final override;

  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override;
  bool sceneEvent(QEvent* ev) override;
  int stepAt(double x) const noexcept;
  void endGesture();

private:
  const Model& m_model;
  SingleOngoingCommandDispatcher<Media::ChangeSteps> m_disp;
  StepsGesture m_gesture;
  int m_playing{-1};
};

}
