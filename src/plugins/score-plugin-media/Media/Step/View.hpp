#pragma once
#include <Process/LayerView.hpp>
#include <score/graphics/RectItem.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <Media/Step/Commands.hpp>
#include <verdigris>

namespace Process
{
struct Context;
}
namespace Media::Step
{
class Model;

class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(const Model&, QGraphicsItem* parent);
  ~View();

  void setBarWidth(double v);

public:
  void change(int arg_1, float arg_2) W_SIGNAL(change, arg_1, arg_2);

private:
  void paint_impl(QPainter* p) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override;

private:
  const Model& m_model;
  double m_barWidth{};
};

class Item final : public score::EmptyRectItem
{
  W_OBJECT(Item)
public:
  explicit Item(const Model&, const Process::Context& ctx, QGraphicsItem* parent);
  ~Item();

  void setBarWidth(double v);

public:
  void change(int arg_1, float arg_2) W_SIGNAL(change, arg_1, arg_2);

private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) final override;

  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override;

private:
  const Model& m_model;
  SingleOngoingCommandDispatcher<Media::ChangeSteps> m_disp;
};

void updateSteps(const Model& m, SingleOngoingCommandDispatcher<Media::ChangeSteps>& disp, std::size_t num, float v);

}
