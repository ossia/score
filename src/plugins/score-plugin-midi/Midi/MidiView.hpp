#pragma once
#include <Midi/MidiProcess.hpp>
#include <Process/LayerView.hpp>

#include <QPainter>
#include <QPainterPath>

#include <verdigris>

namespace Midi
{
class NoteView;
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  View(QGraphicsItem* parent);

  ~View() override;
  double defaultWidth() const noexcept { return m_defaultW; }
  void setDefaultWidth(double w);

  void setRange(int, int);
  std::pair<int, int> range() const { return {m_min, m_max}; }
  NoteData noteAtPos(QPointF point) const;
  int visibleCount() const;

public:
  void deleteRequested() W_SIGNAL(deleteRequested);

private:
  bool canEdit() const;
  void paint_impl(QPainter*) const override;
  void heightChanged(qreal h) override;
  void widthChanged(qreal w) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  QPainterPath m_selectArea;
  double m_defaultW; // Covers the [ 0; 1 ] area
  int m_min{0}, m_max{127};

  QPixmap m_textCache;
  QPixmap m_bgCache;

  mutable std::vector<QPainter::PixmapFragment> m_fragmentCache;
};
}
