#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <QGraphicsSceneMouseEvent>
#include <score/tools/Bind.hpp>
#include <Scenario/Commands/Signature/SignatureCommands.hpp>

namespace Scenario
{

class TimeSignatureHandle
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(TimeSignatureHandle)
public:
  TimeSignatureHandle(const IntervalModel& itv, QGraphicsItem* parent)
    : QGraphicsItem{parent}
  {

  }

  QRectF boundingRect() const final override
  {
    return {0., 0., 10., 15.};
  }

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override
  {
    painter->fillRect(boundingRect(), Qt::gray);
    painter->drawText(QPointF{}, QString{"%1/%2"}.arg(m_sig.upper).arg(m_sig.lower));
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* mv) override
  {
    mv->accept();
    if(mv->button() != Qt::LeftButton)
      remove();
    else
      m_pressX = mv->scenePos().x();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* mv) override
  {
    double delta = mv->scenePos().x() - m_pressX;
    move(delta);
    mv->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* mv) override
  {
    mouseMoveEvent(mv);
    release();
  }

  void setSignature(TimeVal time, Control::time_signature sig)
  {
    m_time = time;
    m_sig = sig;
    update();
  }

  const TimeVal& time() const noexcept { return m_time; }
  const Control::time_signature& signature() const noexcept { return m_sig; }

  void move(double x) W_SIGNAL(move, x);
  void release() W_SIGNAL(release);
  void remove() W_SIGNAL(remove);

private:
  double m_pressX{};
  TimeVal m_time{};
  Control::time_signature m_sig{};
};

  class TimeSignatureItem
      : public QObject
      , public QGraphicsItem
  {
    enum MaxZoomLevel {
      None = 0,
      Whole = 1,
      Half = 2,
      Quarter = 4,
      Eighth = 8,
      Sixteenth = 16,
      Thirtysecond = 32,
      Sixteenfourth = 64
    };
    double m_width{100.};
    ZoomRatio m_ratio{1.};
    const IntervalPresenter& m_itv;
    std::vector<TimeSignatureHandle*> m_handles;
  public:
    TimeSignatureItem(const IntervalPresenter& itv, QGraphicsItem* parent)
      : QGraphicsItem{parent}
      , m_itv{itv}
    {
      con(itv.model(), &IntervalModel::timeSignaturesChanged,
          this, &TimeSignatureItem::handlesChanged);
      handlesChanged();
    }

    void handlesChanged()
    {
      if(m_handles.size() != m_itv.model().timeSignatureMap().size())
      {
        for(auto h : m_handles)
          delete h;
        m_handles.clear();

        for(auto& [time, sig] : m_itv.model().timeSignatureMap())
        {
          auto handle = new TimeSignatureHandle{m_itv.model(), this};
          handle->setPos(time.toPixels(m_ratio), 0.);
          handle->setSignature(time, sig);
          con(*handle, &TimeSignatureHandle::move,
              this, [=] (double x) { moveHandle(*handle, x); });
          con(*handle, &TimeSignatureHandle::release,
              this, [=] { m_itv.context().dispatcher.commit(); });
          con(*handle, &TimeSignatureHandle::remove,
              this, [=] { removeHandle(*handle); }, Qt::QueuedConnection);
          m_handles.push_back(handle);
        }
      }
      else
      {
        auto it = m_handles.begin();
        auto handle_it = m_itv.model().timeSignatureMap().begin();
        while(it != m_handles.end())
        {
          (*it)->setPos(handle_it->first.toPixels(m_ratio), 0.);
          (*it)->setSignature(handle_it->first, handle_it->second);

          ++it;
          ++handle_it;
        }
      }
    }

    void moveHandle(TimeSignatureHandle& handle, double x)
    {
      TimeSignatureMap newsigs = m_itv.model().timeSignatureMap();
      auto it = newsigs.find(handle.time());
      newsigs.erase(it);
      // TODO what if we pass on top of another :|
      newsigs[TimeVal::fromMsecs(x * m_ratio)] = handle.signature();

      m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(m_itv.model(), newsigs);
    }

    void removeHandle(TimeSignatureHandle& handle)
    {
      TimeSignatureMap newsigs = m_itv.model().timeSignatureMap();
      auto it = newsigs.find(handle.time());
      newsigs.erase(it);

      m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(m_itv.model(), newsigs);

    }

    void setZoomRatio(ZoomRatio r)
    {
      m_ratio = r;
      auto it = m_handles.begin();
      auto handle_it = m_itv.model().timeSignatureMap().begin();
      while(it != m_handles.end())
      {
        auto& [time, sig] = *handle_it;

        (*it)->setPos(time.toPixels(m_ratio), 0.);
        (*it)->setSignature(time, sig);

        ++it;
        ++handle_it;
      }

    }

    void setWidth(double w)
    {
      prepareGeometryChange();
      m_width = w;
    }

    QRectF boundingRect() const final override
    {
      return {0., 0., m_width, 10.};
    }

    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override
    {

    }
    /*
    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override
    {
      painter->setPen(Qt::white);

      auto zoomLevel = Whole;
      double spacing = m_itv.zoomRatio();

      for(auto& [time, sig] : m_itv.model().timeSignatureMap())
      {

      }
    }*/
  };
}


