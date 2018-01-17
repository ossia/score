#pragma once
#include <Midi/MidiProcess.hpp>
#include <Process/LayerView.hpp>
#include <QPainterPath>

namespace Midi
{
class NoteView;
class View final : public Process::LayerView
{
  Q_OBJECT
public:
  View(QGraphicsItem* parent);

  ~View();
  double defaultWidth() const;
  void setDefaultWidth(double w);

  void setRange(int, int);
  std::pair<int, int> range() const { return {m_min, m_max}; }
  NoteData noteAtPos(QPointF point) const;

Q_SIGNALS:
  void deleteRequested();

private:
  void paint_impl(QPainter*) const override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;

  QPainterPath m_selectArea;
  double m_defaultW; // Covers the [ 0; 1 ] area
  int m_min{0}, m_max{127};
};
}
