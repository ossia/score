#pragma once
#include <Process/LayerView.hpp>

#include <Midi/MidiProcess.hpp>

#include <QPainter>
#include <QPainterPath>
#include <QPointer>

#include <functional>

#include <verdigris>

namespace Midi
{
class BarGrid;
class NoteView;
class SCORE_PLUGIN_MIDI_EXPORT View final : public Process::LayerView
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

  //! Musical mode's bar lines are drawn over the rows, under the notes.
  void setBarGrid(BarGrid* grid);

  //! Where a time-related x lands: what magnetism makes of it, or `x`.
  void setSnap(std::function<double(double)> snap) { m_snap = std::move(snap); }
  double snap(double x) const { return m_snap ? m_snap(x) : x; }

public:
  void deleteRequested() W_SIGNAL(deleteRequested);
  //! Up / Down: a semitone; with Shift, an octave.
  void transposeRequested(int semitones) W_SIGNAL(transposeRequested, semitones);

private:
  bool canEdit() const;
  void paint_impl(QPainter*) const override;
  void heightChanged(qreal h) override;
  void widthChanged(qreal w) override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void updateBackground(double height);

  QPainterPath m_selectArea;
  double m_defaultW; // Covers the [ 0; 1 ] area
  int m_min{0}, m_max{127};

  QPixmap m_textCache;
  friend class NoteNames;
  //! The note names, over the notes, at the left of what is shown
  QGraphicsItem* m_names{};
  QPixmap m_bgCache;

  QPointer<BarGrid> m_barGrid;
  std::function<double(double)> m_snap;

  mutable std::vector<QPainter::PixmapFragment> m_fragmentCache;
};
}
