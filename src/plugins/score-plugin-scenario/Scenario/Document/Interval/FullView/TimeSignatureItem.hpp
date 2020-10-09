#pragma once
#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Commands/Signature/SignatureCommands.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QObject>
#include <QPainter>
#include <QTextLayout>

#include <Magnetism/MagnetismAdjuster.hpp>
#include <ossia/detail/algorithms.hpp>

#include <verdigris>

namespace Scenario
{

class LineTextItem final : public QGraphicsTextItem
{
  W_OBJECT(LineTextItem)
public:
  LineTextItem(QGraphicsItem* parent) noexcept : QGraphicsTextItem{parent}
  {
    setFlags(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsSelectable | flags());
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setDefaultTextColor(Qt::black);
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::black);
    painter->setBrush(Qt::white);
    painter->drawRect(boundingRect());

    QGraphicsTextItem::paint(painter, option, widget);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  void keyPressEvent(QKeyEvent* ev) override
  {
    ev->accept();
    switch (ev->key())
    {
      case Qt::Key_Left:
      {
        auto c = textCursor();
        c.setPosition(std::max(0, c.position() - 1));
        setTextCursor(c);
        return;
      }
      case Qt::Key_Right:
      {
        auto c = textCursor();
        c.setPosition(std::min(toPlainText().size(), c.position() + 1));
        setTextCursor(c);
        return;
      }
      case Qt::Key_Enter:
      case Qt::Key_Return:
        done(toPlainText());
        return;
      case Qt::Key_Escape:
        done({});
        return;
      default:
        QGraphicsTextItem::keyPressEvent(ev);
    }
  }

  void keyReleaseEvent(QKeyEvent* ev) override
  {
    ev->accept();
    QGraphicsTextItem::keyPressEvent(ev);
  }

  void focusOutEvent(QFocusEvent* event) override
  {
    done(toPlainText());
    QGraphicsTextItem::focusOutEvent(event);
  }

  void done(QString s) W_SIGNAL(done, s)
};

class TimeSignatureHandle : public QObject, public QGraphicsItem
{
  W_OBJECT(TimeSignatureHandle)
public:
  TimeSignatureHandle(const IntervalModel& itv, QGraphicsItem* parent) : QGraphicsItem{parent}
  {
    setFlag(ItemIsSelectable, true);
  }

  ~TimeSignatureHandle() { }

  QRectF boundingRect() const final override
  {
    return {0., 0., std::max(20., 12. + m_rect.width()), std::max(20., 3. + m_rect.height())};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    if (m_visible)
    {
      painter->drawPixmap(QPointF{0., 2.}, Process::Pixmaps::instance().metricHandle);
      painter->drawPixmap(QPointF{10., 3.}, m_signature);
    }
  }

  void setSignature(TimeVal time, Control::time_signature sig)
  {
    m_time = time;
    if (sig != m_sig)
    {
      m_sig = sig;
      updateImpl();
    }
    update();
  }

  const TimeVal& time() const noexcept { return m_time; }
  const Control::time_signature& signature() const noexcept { return m_sig; }

  void move(double originalPos, double delta) W_SIGNAL(move, originalPos, delta);
  void press() W_SIGNAL(press);
  void release() W_SIGNAL(release);
  void remove() W_SIGNAL(remove);
  void signatureChange(Control::time_signature sig) W_SIGNAL(signatureChange, sig);

  bool pressed{};

protected:
  void updateImpl()
  {
    prepareGeometryChange();

    auto& skin = score::Skin::instance();
    auto& m_font = skin.Medium8Pt;

    {
      const auto str = QString{"%1/%2"}.arg(m_sig.upper).arg(m_sig.lower);
      QTextLayout layout(str, m_font);
      layout.beginLayout();
      auto line = layout.createLine();
      layout.endLayout();

      m_rect = line.naturalTextRect();
      auto r = line.glyphRuns();

      if (r.size() > 0)
      {
        double ratio = qApp->devicePixelRatio();
        auto m_line = QImage(
            m_rect.width() * ratio, m_rect.height() * ratio, QImage::Format_ARGB32_Premultiplied);
        m_line.setDevicePixelRatio(ratio);
        m_line.fill(Qt::transparent);

        {
          QPainter p{&m_line};
          p.setRenderHint(QPainter::Antialiasing, false);
          p.setRenderHint(QPainter::TextAntialiasing, false);

          p.setPen(skin.Light.main.pen0);
          p.setBrush(skin.NoBrush);
          p.drawGlyphRun(QPointF{0, 0}, r[0]);
        }
        m_signature = QPixmap::fromImage(m_line);
      }
    }

    update();
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* mv) override
  {
    m_visible = false;
    prepareGeometryChange();
    update();

    auto& skin = score::Skin::instance();
    auto& font = skin.Medium8Pt;

    auto item = new LineTextItem{this};
    item->setTextInteractionFlags(Qt::TextEditable);
    item->setPlainText(QString{"%1/%2"}.arg(m_sig.upper).arg(m_sig.lower));

    item->setFont(font);
    item->setFocus(Qt::OtherFocusReason);

    connect(
        item,
        &LineTextItem::done,
        this,
        [this, item](const QString& s) {
          if (auto sig = Control::get_time_signature(s.toStdString()))
          {
            signatureChange(*sig);
          }
          item->deleteLater();

          m_visible = true;
          prepareGeometryChange();
          update();
        },
        Qt::QueuedConnection);

    mv->accept();
  }
  TimeVal m_time{};
  Control::time_signature m_sig{0, 0};
  QPixmap m_signature;
  QRectF m_rect;

public:
  bool m_visible{true};
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

  void mousePressEvent(QGraphicsSceneMouseEvent* mv) override
  {
    mv->accept();
    if (mv->button() != Qt::LeftButton)
    {
      remove();
    }
    else
    {
      pressed = true;
      m_origItemX = this->x();
      m_pressX = mv->scenePos().x();
      press();
    }
    QGraphicsItem::mousePressEvent(mv);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* mv) override
  {
    double delta = mv->scenePos().x() - m_pressX;
    if (delta != 0)
    {
      move(m_origItemX, delta);
    }
    mv->accept();

    QGraphicsItem::mouseMoveEvent(mv);
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* mv) override
  {
    mouseMoveEvent(mv);
    pressed = false;
    release();
    QGraphicsItem::mouseReleaseEvent(mv);
  }
};

class TimeSignatureItem : public QObject, public QGraphicsItem
{
public:
  TimeSignatureItem(const IntervalPresenter& itv, QGraphicsItem* parent)
      : QGraphicsItem{parent}
      , m_itv{itv}
      , m_magnetic{(Process::MagnetismAdjuster&)m_itv.context()
                       .app.interfaces<Process::MagnetismAdjuster>()}
  {
    setZValue(200);
    setCursor(Qt::CrossCursor);
    setFlag(ItemHasNoContents, true);
    setFlag(ItemClipsChildrenToShape, false);

    handlesChanged();
  }

  void createHandle(TimeVal time, Control::time_signature sig)
  {
    assert(m_model);
    TimeSignatureHandle* handle{};

    if (time == TimeVal::zero())
    {
      // The first time handle cannot move or change
      handle = new FixedHandle{*m_model, this};
    }
    else
    {
      // Other handles are free
      handle = new MovableHandle{*m_model, this};

      con(*handle, &TimeSignatureHandle::press, this, [=] {
        assert(m_model);
        m_origHandles = m_model->timeSignatureMap();
        m_origTime = handle->time();
        m_origSig = handle->signature();
      });
      con(*handle, &TimeSignatureHandle::move, this, [=](double originalPos, double delta) {
        assert(m_model);
        if (handle->m_visible)
          moveHandle(*handle, originalPos, delta);
      });
      con(*handle, &TimeSignatureHandle::release, this, [=] {
        assert(m_model);
        if (handle->m_visible)
        {
          m_origHandles.clear();
          m_itv.context().dispatcher.commit();
        }
      });
      con(
          *handle,
          &TimeSignatureHandle::remove,
          this,
          [=] {
            assert(m_model);
            if (handle->m_visible)
              removeHandle(*handle);
          },
          Qt::QueuedConnection);
    }

    handle->setPos((time - m_timeDelta).toPixels(m_ratio), 0.);
    handle->setSignature(time, sig);

    con(
        *handle,
        &TimeSignatureHandle::signatureChange,
        this,
        [=](Control::time_signature sig) {
          assert(m_model);
          auto signatures = m_model->timeSignatureMap();

          signatures.at(handle->time()) = sig;

          m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(
              *m_model, signatures);
        },
        Qt::QueuedConnection);

    m_handles.push_back(handle);
  }

  void setZoomRatio(ZoomRatio r)
  {
    if (!m_model)
      return;

    if (m_ratio != r)
    {
      m_ratio = r;

      auto it = m_handles.begin();
      auto handle_it = m_model->timeSignatureMap().begin();
      while (it != m_handles.end())
      {
        auto& [time, sig] = *handle_it;

        (*it)->setPos((time - m_timeDelta).toPixels(m_ratio), 0.);
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

  void setModel(const IntervalModel* model, TimeVal delta)
  {
    if (model != m_model)
    {
      if (m_model)
        disconnect(
            m_model,
            &IntervalModel::timeSignaturesChanged,
            this,
            &TimeSignatureItem::handlesChanged);

      m_model = model;

      if (m_model)
      {
        connect(
            m_model,
            &IntervalModel::timeSignaturesChanged,
            this,
            &TimeSignatureItem::handlesChanged);
      }

      for (auto h : m_handles)
        delete h;
      m_handles.clear();
    }
    m_timeDelta = delta;
    handlesChanged();
  }

private:
  void handlesChanged()
  {
    if (!m_model)
      return;

    const auto& signatures = m_model->timeSignatureMap();
    if (m_handles.size() > signatures.size())
    {
      // Removed handles
      for (auto it = m_handles.begin(); it != m_handles.end();)
      {
        // TODO what if we undo creation while pressing
        // we should prevent undo / redo while doing an action...
        if (signatures.find((*it)->time()) == signatures.end())
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
    else if (m_handles.size() < signatures.size())
    {
      // Created handles
      for (auto h : m_handles)
        delete h;
      m_handles.clear();

      for (auto& [time, sig] : signatures)
      {
        createHandle(time, sig);
      }
    }
    else
    {
      for (auto h : m_handles)
        if (h->pressed)
          return;

      auto it = m_handles.begin();
      auto handle_it = signatures.begin();
      while (it != m_handles.end())
      {
        (*it)->setPos((handle_it->first - m_timeDelta).toPixels(m_ratio), 0.);
        (*it)->setSignature(handle_it->first, handle_it->second);

        ++it;
        ++handle_it;
      }
    }
  }

  void moveHandle(TimeSignatureHandle& handle, double originalPos, double delta)
  {
    const double x = originalPos + delta;
    // TODO what if we pass on top of another :|

    // Find leftmost signature
    const auto msecs = TimeVal::fromPixels(x, m_ratio);

    const auto [new_time, showSnap] = m_magnetic.getPosition(m_model, msecs);

    // Replace it in the signatures
    TimeSignatureMap signatures = m_origHandles;
    auto it = signatures.find(m_origTime);
    if (it == signatures.end())
    {
      qWarning("Time signature not found");
    }
    else
    {
      signatures.erase(it);
    }
    signatures[new_time] = m_origSig;

    // Set new position for the handle
    handle.setX(new_time.toPixels(m_ratio));
    handle.setSignature(new_time, handle.signature());

    m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(*m_model, signatures);
  }

  void removeHandle(TimeSignatureHandle& handle)
  {
    if (handle.pressed)
      return;
    TimeSignatureMap signatures = m_model->timeSignatureMap();
    auto it = signatures.find(handle.time());
    signatures.erase(it);

    CommandDispatcher<> disp{m_itv.context().commandStack};
    disp.submit<Scenario::Command::SetTimeSignatures>(*m_model, signatures);
  }

  QRectF boundingRect() const final override { return {0., 0., m_width, 20.}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
  }

  void requestNewHandle(QPointF pos)
  {
    assert(m_model);
    auto signatures = m_model->timeSignatureMap();
    signatures[TimeVal::fromPixels(pos.x(), m_ratio)] = Control::time_signature{4, 4};
    CommandDispatcher<> disp{m_itv.context().commandStack};
    disp.submit<Scenario::Command::SetTimeSignatures>(*m_model, signatures);
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
  {
    if (ossia::any_of(m_handles, [](auto handle) { return handle->pressed; }))
      return;

    QMenu menu;
    auto act = menu.addAction("Add signature change");
    connect(act, &QAction::triggered, this, [this, pos = event->pos()] { requestNewHandle(pos); });

    menu.exec(event->screenPos());
  }

  double m_width{100.};
  ZoomRatio m_ratio{1.};

  const IntervalPresenter& m_itv;
  const IntervalModel* m_model{};
  TimeVal m_timeDelta;
  Process::MagnetismAdjuster& m_magnetic;

  std::vector<TimeSignatureHandle*> m_handles;
  TimeSignatureMap m_origHandles{};

  TimeVal m_origTime{};
  Control::time_signature m_origSig{};
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
