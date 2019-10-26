#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <score/tools/Bind.hpp>
#include <Scenario/Commands/Signature/SignatureCommands.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

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

  ~TimeSignatureHandle()
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
    pressed = true;
    mv->accept();
    if(mv->button() != Qt::LeftButton)
    {
      remove();
    }
    else
    {
      m_origItemX = this->x();
      m_pressX = mv->scenePos().x();
      press();
    }
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* mv) override
  {
    double delta = mv->scenePos().x() - m_pressX;
    if(delta != 0)
      move(m_origItemX + delta);
    mv->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* mv) override
  {
    mouseMoveEvent(mv);
    pressed = false;
    release();
  }

  void setSignature(TimeVal time, Control::time_signature sig)
  {
    m_time = time;
    if(sig != m_sig)
    {
      m_sig = sig;
    }
    update();
  }

  const TimeVal& time() const noexcept { return m_time; }
  const Control::time_signature& signature() const noexcept { return m_sig; }

  void move(double x) W_SIGNAL(move, x);
  void press() W_SIGNAL(press);
  void release() W_SIGNAL(release);
  void remove() W_SIGNAL(remove);

  bool pressed{};

private:
  double m_origItemX{};
  double m_pressX{};
  TimeVal m_time{};
  Control::time_signature m_sig{};
};

  class TimeSignatureItem
      : public QObject
      , public QGraphicsItem
  {
    double m_width{100.};
    ZoomRatio m_ratio{1.};
    const IntervalPresenter& m_itv;
    std::vector<TimeSignatureHandle*> m_handles;
    TimeSignatureMap m_origHandles{};
    TimeVal m_origTime{};
    Control::time_signature m_origSig{};
  public:
    TimeSignatureItem(const IntervalPresenter& itv, QGraphicsItem* parent)
      : QGraphicsItem{parent}
      , m_itv{itv}
    {
      setFlag(ItemHasNoContents, true);

      con(itv.model(), &IntervalModel::timeSignaturesChanged,
          this, &TimeSignatureItem::handlesChanged);
      handlesChanged();
    }

    void createHandle(TimeVal time, Control::time_signature sig)
    {
      auto handle = new TimeSignatureHandle{m_itv.model(), this};

      handle->setPos(time.toPixels(m_ratio), 0.);
      handle->setSignature(time, sig);

      con(*handle, &TimeSignatureHandle::press,
          this, [=] {
        m_origHandles = this->m_itv.model().timeSignatureMap();
        m_origTime = handle->time();
        m_origSig = handle->signature();
      });
      con(*handle, &TimeSignatureHandle::move,
          this, [=] (double x) {
        moveHandle(*handle, x);
      });
      con(*handle, &TimeSignatureHandle::release,
          this, [=] {
        m_origHandles.clear();
        m_itv.context().dispatcher.commit();
      });
      con(*handle, &TimeSignatureHandle::remove,
          this, [=] {
        removeHandle(*handle);
      }, Qt::QueuedConnection);

      m_handles.push_back(handle);
    }

    void setZoomRatio(ZoomRatio r)
    {
      if(m_ratio != r)
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
    }

    void setWidth(double w)
    {
      prepareGeometryChange();
      m_width = w;
    }

  private:
    void handlesChanged()
    {
      const auto& signatures = m_itv.model().timeSignatureMap();
      if(m_handles.size() > signatures.size())
      {
        // Removed handles
        for(auto it = m_handles.begin(); it != m_handles.end(); )
        {
          // TODO what if we undo creation while pressing
          // we should prevent undo / redo while doing an action...
          if(signatures.find((*it)->time()) == signatures.end())
          {
            SCORE_ASSERT(!((*it)->pressed));
            delete *it;
            it = m_handles.erase(it);
          }
          else
          {
            ++it;
          }
        }
      }
      else if(m_handles.size() < signatures.size())
      {
        // Created handles
        for(auto h : m_handles)
          delete h;
        m_handles.clear();

        for(auto& [time, sig] : signatures)
        {
          createHandle(time, sig);
        }
      }
      else
      {
        for(auto h : m_handles)
          if(h->pressed)
            return;

        auto it = m_handles.begin();
        auto handle_it = signatures.begin();
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
      TimeSignatureMap signatures = m_origHandles;
      auto it = signatures.find(m_origTime);
      signatures.erase(it);
      // TODO what if we pass on top of another :|
      auto new_time = TimeVal::fromMsecs(x * m_ratio);
      signatures[new_time] = m_origSig;
      handle.setX(x);
      handle.setSignature(new_time, handle.signature());

      m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(m_itv.model(), signatures);
    }

    void removeHandle(TimeSignatureHandle& handle)
    {
      TimeSignatureMap signatures = m_itv.model().timeSignatureMap();
      auto it = signatures.find(handle.time());
      signatures.erase(it);

      m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(m_itv.model(), signatures);
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

    void requestNewHandle(QPointF pos)
    {
      auto signatures = m_itv.model().timeSignatureMap();
      signatures[TimeVal::fromMsecs(pos.x() * m_ratio)] = Control::time_signature{4, 4};
      CommandDispatcher<> disp{m_itv.context().commandStack};
      disp.submit<Scenario::Command::SetTimeSignatures>(m_itv.model(), signatures);
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
    {
      QMenu menu;
      auto act = menu.addAction("Add signature change");
      connect(act, &QAction::triggered, this, [this, pos=event->pos()] {
        requestNewHandle(pos);
      });

      menu.exec(event->screenPos());
    }
};
}


