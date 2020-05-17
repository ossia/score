#pragma once
#include <Effect/EffectPainting.hpp>

class QGraphicsSceneMouseEvent;
namespace Dataflow
{
class PortItem;
}
namespace Process
{
class ProcessModel;
struct LayerContext;
class LayerFactoryList;
}
namespace Media
{

class View;
class EffectItem final : public ::Effect::ItemBase
{
  const View& m_view;
  const Process::ProcessModel& m_model;
  const Process::LayerContext& m_context;

public:
  EffectItem(
      const View& view,
      const Process::ProcessModel& effect,
      const Process::LayerContext& ctx,
      const Process::LayerFactoryList& fact,
      QGraphicsItem* parent);

  static inline qreal litHeight{};

private:
  void updateSize();
  void setSize(QSizeF sz);
  void resetInlets();
  void resetOutlets();

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  score::ResizeableItem* m_fx{};
  std::vector<Dataflow::PortItem*> m_inlets;
  std::vector<Dataflow::PortItem*> m_outlets;
};
}

namespace score::mime
{
inline constexpr auto effect()
{
  return "application/x-score-fxdata";
}
}
