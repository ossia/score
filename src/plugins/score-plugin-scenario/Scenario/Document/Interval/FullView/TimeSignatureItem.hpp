#pragma once
#include <Scenario/Document/Interval/TimeSignatureMap.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>

#include <ossia/editor/scenario/time_signature.hpp>

#include <QObject>
#include <QGraphicsTextItem>

#include <verdigris>

namespace Process
{
class MagnetismAdjuster;
}

namespace Scenario
{
class IntervalModel;
class FullViewIntervalPresenter;
class LineTextItem final : public QGraphicsTextItem
{
  W_OBJECT(LineTextItem)
public:
  LineTextItem(QGraphicsItem* parent) noexcept;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void keyPressEvent(QKeyEvent* ev) override;
  void keyReleaseEvent(QKeyEvent* ev) override;
  void focusOutEvent(QFocusEvent* event) override;
  void done(QString s) W_SIGNAL(done, s)
};

class TimeSignatureHandle
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(TimeSignatureHandle)
public:
  explicit TimeSignatureHandle(QGraphicsItem* parent);
  ~TimeSignatureHandle();

  QRectF boundingRect() const final override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setSignature(TimeVal time, ossia::time_signature sig);

  const TimeVal& time() const;
  const ossia::time_signature& signature() const;

  void move(double originalPos, double delta)
      W_SIGNAL(move, originalPos, delta);
  void press() W_SIGNAL(press);
  void release() W_SIGNAL(release);
  void remove() W_SIGNAL(remove);
  void signatureChange(ossia::time_signature sig)
      W_SIGNAL(signatureChange, sig);

  bool pressed{};

protected:
  void updateImpl();

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* mv) override;
  TimeVal m_time{};
  ossia::time_signature m_sig{0, 0};
  QPixmap m_signature;
  QRectF m_rect;

public:
  bool m_visible{true};
};

class StartMarker
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(StartMarker)
public:
  explicit StartMarker(QGraphicsItem* parent);
  ~StartMarker();

  QRectF boundingRect() const final override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void move(double originalPos, double delta)
      W_SIGNAL(move, originalPos, delta);
  void press() W_SIGNAL(press);
  void release() W_SIGNAL(release);
  void remove() W_SIGNAL(remove);

  void mousePressEvent(QGraphicsSceneMouseEvent* mv) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* mv) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* mv) override;

  bool pressed{};
private:
  double m_origItemX{};
  double m_pressX{};
  TimeVal m_time{};
};

class FixedHandle final : public TimeSignatureHandle
{
public:
  using TimeSignatureHandle::TimeSignatureHandle;
};

class MovableHandle final : public TimeSignatureHandle
{
public:
  using TimeSignatureHandle::TimeSignatureHandle;

private:
  double m_origItemX{};
  double m_pressX{};

  void mousePressEvent(QGraphicsSceneMouseEvent* mv) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* mv) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* mv) override;
};

class TimeSignatureItem
    : public QObject
    , public QGraphicsItem
{
public:
  TimeSignatureItem(const FullViewIntervalPresenter& itv, QGraphicsItem* parent);

  void createHandle(TimeVal time, ossia::time_signature sig);
  void setZoomRatio(ZoomRatio r);
  void setWidth(double w);
  void setModel(const IntervalModel* model, TimeVal delta);
  void updateStartMarker();

private:
  void handlesChanged();

  void
  moveHandle(TimeSignatureHandle& handle, double originalPos, double delta);

  void removeHandle(TimeSignatureHandle& handle);
  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  void requestNewHandle(QPointF pos);
  void setStartMarker(QPointF pos);
  void removeStartMarker();
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

  double m_width{100.};
  ZoomRatio m_ratio{1.};

  const FullViewIntervalPresenter& m_itv;
  const IntervalModel* m_model{};
  TimeVal m_timeDelta;
  Process::MagnetismAdjuster& m_magnetic;

  StartMarker* m_start{};
  std::vector<TimeSignatureHandle*> m_handles;
  TimeSignatureMap m_origHandles{};

  TimeVal m_origTime{};
  ossia::time_signature m_origSig{};
};

class FullViewIntervalPresenter;
struct Timebars
{
  Timebars(FullViewIntervalPresenter& self);

  TimeSignatureItem timebar;

  LightBars lightBars;
  LighterBars lighterBars;
  std::vector<TimeVal> magneticTimings;
};
}
