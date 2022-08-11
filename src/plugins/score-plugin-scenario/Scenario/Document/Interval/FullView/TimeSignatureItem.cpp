#include "TimeSignatureItem.hpp"

#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <Scenario/Commands/Signature/SignatureCommands.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Magnetism/MagnetismAdjuster.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QObject>
#include <QPainter>
#include <QTextLayout>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TimeSignatureHandle)
W_OBJECT_IMPL(Scenario::LineTextItem)
W_OBJECT_IMPL(Scenario::StartMarker)

namespace Scenario
{

LineTextItem::LineTextItem(QGraphicsItem* parent) noexcept
    : QGraphicsTextItem{parent}
{
  setFlags(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsSelectable | flags());
  setTextInteractionFlags(Qt::TextEditorInteraction);
  setDefaultTextColor(Qt::black);
}

void LineTextItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(Qt::black);
  painter->setBrush(Qt::white);
  painter->drawRect(boundingRect());

  QGraphicsTextItem::paint(painter, option, widget);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void LineTextItem::keyPressEvent(QKeyEvent* ev)
{
  ev->accept();
  switch(ev->key())
  {
    case Qt::Key_Left: {
      auto c = textCursor();
      c.setPosition(std::max(0, c.position() - 1));
      setTextCursor(c);
      return;
    }
    case Qt::Key_Right: {
      auto c = textCursor();
      c.setPosition(ossia::min(int64_t(toPlainText().size()), c.position() + 1));
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

void LineTextItem::keyReleaseEvent(QKeyEvent* ev)
{
  ev->accept();
  QGraphicsTextItem::keyPressEvent(ev);
}

void LineTextItem::focusOutEvent(QFocusEvent* event)
{
  done(toPlainText());
  QGraphicsTextItem::focusOutEvent(event);
}

StartMarker::StartMarker(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setToolTip(QObject::tr("Start marker"));
  setFlag(ItemIsSelectable, true);
}

StartMarker::~StartMarker() { }

static constexpr double startMarkerDiam = 12.;
QRectF StartMarker::boundingRect() const
{
  return {0., 0., startMarkerDiam, startMarkerDiam};
}

const QPainterPath& startMarkerPath()
{
  static const QPainterPath p = [] {
    QPainterPath p;
    p.moveTo(startMarkerDiam / 2., startMarkerDiam);
    p.arcTo(QRectF{0, 0, startMarkerDiam, startMarkerDiam}, 270, 180);
    p.closeSubpath();
    return p;
  }();
  return p;
}

void StartMarker::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->translate(-startMarkerDiam / 2., 0.);
  painter->fillPath(startMarkerPath(), score::Skin::instance().Gray);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void StartMarker::mousePressEvent(QGraphicsSceneMouseEvent* mv)
{
  mv->accept();
  if(mv->button() != Qt::LeftButton)
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

void StartMarker::mouseMoveEvent(QGraphicsSceneMouseEvent* mv)
{
  double delta = mv->scenePos().x() - m_pressX;
  if(delta != 0)
  {
    move(m_origItemX, delta);
  }
  mv->accept();

  QGraphicsItem::mouseMoveEvent(mv);
}

void StartMarker::mouseReleaseEvent(QGraphicsSceneMouseEvent* mv)
{
  mouseMoveEvent(mv);
  pressed = false;
  release();
  QGraphicsItem::mouseReleaseEvent(mv);
}

TimeSignatureHandle::TimeSignatureHandle(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setToolTip(QObject::tr(
      "Time signature handle\nDrag to displace, double-click to change the signature."));
  setFlag(ItemIsSelectable, true);
}

TimeSignatureHandle::~TimeSignatureHandle() { }

QRectF TimeSignatureHandle::boundingRect() const
{
  return {
      0., 0., std::max(20., 12. + m_rect.width()), std::max(20., 3. + m_rect.height())};
}

void TimeSignatureHandle::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(m_visible)
  {
    painter->drawPixmap(QPointF{0., 2.}, Process::Pixmaps::instance().metricHandle);
    painter->drawPixmap(QPointF{10., 3.}, m_signature);
  }
}

void TimeSignatureHandle::setSignature(TimeVal time, ossia::time_signature sig)
{
  m_time = time;
  if(sig != m_sig)
  {
    m_sig = sig;
    updateImpl();
  }
  update();
}

const TimeVal& TimeSignatureHandle::time() const
{
  return m_time;
}

const ossia::time_signature& TimeSignatureHandle::signature() const
{
  return m_sig;
}

void TimeSignatureHandle::updateImpl()
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

    if(r.size() > 0)
    {
      auto m_line = newImage(m_rect.width(), m_rect.height());

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

void TimeSignatureHandle::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* mv)
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
      item, &LineTextItem::done, this,
      [this, item](const QString& s) {
    if(auto sig = ossia::get_time_signature(s.toStdString()))
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

void MovableHandle::mousePressEvent(QGraphicsSceneMouseEvent* mv)
{
  mv->accept();
  if(mv->button() != Qt::LeftButton)
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

void MovableHandle::mouseMoveEvent(QGraphicsSceneMouseEvent* mv)
{
  double delta = mv->scenePos().x() - m_pressX;
  if(delta != 0)
  {
    move(m_origItemX, delta);
  }
  mv->accept();

  QGraphicsItem::mouseMoveEvent(mv);
}

void MovableHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent* mv)
{
  mouseMoveEvent(mv);
  pressed = false;
  release();
  QGraphicsItem::mouseReleaseEvent(mv);
}

TimeSignatureItem::TimeSignatureItem(
    const FullViewIntervalPresenter& itv, QGraphicsItem* parent)
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

void TimeSignatureItem::createHandle(TimeVal time, ossia::time_signature sig)
{
  assert(m_model);
  TimeSignatureHandle* handle{};

  if(time == TimeVal::zero())
  {
    // The first time handle cannot move or change
    handle = new FixedHandle{this};
  }
  else
  {
    // Other handles are free
    handle = new MovableHandle{this};

    con(*handle, &TimeSignatureHandle::press, this, [this, handle] {
      assert(m_model);
      m_origHandles = m_model->timeSignatureMap();
      m_origTime = handle->time();
      m_origSig = handle->signature();
    });
    con(*handle, &TimeSignatureHandle::move, this,
        [this, handle](double originalPos, double delta) {
      assert(m_model);
      if(handle->m_visible)
        moveHandle(*handle, originalPos, delta);
    });
    con(*handle, &TimeSignatureHandle::release, this, [this, handle] {
      assert(m_model);
      if(handle->m_visible)
      {
        m_origHandles.clear();
        m_itv.context().dispatcher.commit();
      }
    });
    con(
        *handle, &TimeSignatureHandle::remove, this,
        [this, handle] {
      assert(m_model);
      if(handle->m_visible)
        removeHandle(*handle);
        },
        Qt::QueuedConnection);
  }

  handle->setPos((time - m_timeDelta).toPixels(m_ratio), 0.);
  handle->setSignature(time, sig);

  con(
      *handle, &TimeSignatureHandle::signatureChange, this,
      [this, handle](ossia::time_signature sig) {
    assert(m_model);
    auto signatures = m_model->timeSignatureMap();

    signatures.at(handle->time()) = sig;

    m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(
        *m_model, signatures);

    m_itv.context().dispatcher.commit();
      },
      Qt::QueuedConnection);

  m_handles.push_back(handle);
}

void TimeSignatureItem::setZoomRatio(ZoomRatio r)
{
  if(!m_model)
    return;

  if(m_ratio != r)
  {
    m_ratio = r;

    updateStartMarker();

    auto it = m_handles.begin();
    auto handle_it = m_model->timeSignatureMap().begin();
    while(it != m_handles.end())
    {
      auto& [time, sig] = *handle_it;

      (*it)->setPos((time - m_timeDelta).toPixels(m_ratio), 0.);
      (*it)->setSignature(time, sig);

      ++it;
      ++handle_it;
    }
  }
}

void TimeSignatureItem::setWidth(double w)
{
  prepareGeometryChange();
  m_width = w;
}

void TimeSignatureItem::setModel(const IntervalModel* model, TimeVal delta)
{
  if(model != m_model)
  {
    if(m_model)
    {
      disconnect(
          m_model, &IntervalModel::timeSignaturesChanged, this,
          &TimeSignatureItem::handlesChanged);
      disconnect(
          m_model, &IntervalModel::startMarkerChanged, this,
          &TimeSignatureItem::updateStartMarker);
    }

    m_model = model;

    if(m_model)
    {
      connect(
          m_model, &IntervalModel::timeSignaturesChanged, this,
          &TimeSignatureItem::handlesChanged);
      connect(
          m_model, &IntervalModel::startMarkerChanged, this,
          &TimeSignatureItem::updateStartMarker);
    }

    for(auto h : m_handles)
      delete h;
    m_handles.clear();
    delete m_start;
    m_start = nullptr;
  }
  m_timeDelta = delta;
  handlesChanged();
}

void TimeSignatureItem::updateStartMarker()
{
  if(!m_model)
    return;
  const TimeVal st = m_model->startMarker();
  if(m_start && st != TimeVal::zero())
  {
    m_start->setPos(st.toPixels(m_ratio), 12.);
  }
  else if(m_start && st == TimeVal::zero())
  {
    delete m_start;
    m_start = nullptr;
  }
  else if(!m_start && st != TimeVal::zero())
  {
    if(!m_start)
      m_start = new StartMarker{this};
    m_start->setPos(st.toPixels(m_ratio), 12.);
  }
}

void TimeSignatureItem::handlesChanged()
{
  if(!m_model)
    return;

  // Start marker
  updateStartMarker();

  // Time signature handles
  const auto& signatures = m_model->timeSignatureMap();
  if(m_handles.size() > signatures.size())
  {
    // Removed handles
    for(auto it = m_handles.begin(); it != m_handles.end();)
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
      (*it)->setPos((handle_it->first - m_timeDelta).toPixels(m_ratio), 0.);
      (*it)->setSignature(handle_it->first, handle_it->second);

      ++it;
      ++handle_it;
    }
  }
}

void TimeSignatureItem::moveHandle(
    TimeSignatureHandle& handle, double originalPos, double delta)
{
  const double x = originalPos + delta;
  // TODO what if we pass on top of another :|

  // Find leftmost signature
  const auto msecs = TimeVal::fromPixels(x, m_ratio);

  const auto [new_time, showSnap] = m_magnetic.getPosition(m_model, msecs);

  // Replace it in the signatures
  TimeSignatureMap signatures = m_origHandles;
  auto it = signatures.find(m_origTime);
  if(it == signatures.end())
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

  m_itv.context().dispatcher.submit<Scenario::Command::SetTimeSignatures>(
      *m_model, signatures);
}

void TimeSignatureItem::removeHandle(TimeSignatureHandle& handle)
{
  if(handle.pressed)
    return;
  TimeSignatureMap signatures = m_model->timeSignatureMap();
  auto it = signatures.find(handle.time());
  signatures.erase(it);

  CommandDispatcher<> disp{m_itv.context().commandStack};
  disp.submit<Scenario::Command::SetTimeSignatures>(*m_model, signatures);
}

QRectF TimeSignatureItem::boundingRect() const
{
  return {0., 0., m_width, 20.};
}

void TimeSignatureItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

void TimeSignatureItem::requestNewHandle(QPointF pos)
{
  assert(m_model);
  auto signatures = m_model->timeSignatureMap();
  signatures[TimeVal::fromPixels(pos.x(), m_ratio)] = ossia::time_signature{4, 4};
  CommandDispatcher<> disp{m_itv.context().commandStack};
  disp.submit<Scenario::Command::SetTimeSignatures>(*m_model, signatures);
}

void TimeSignatureItem::setStartMarker(QPointF pos)
{
  assert(m_model);
  ((IntervalModel*)m_model)
      ->setStartMarker(TimeVal::fromPixels(pos.x(), m_itv.zoomRatio()));
}

void TimeSignatureItem::removeStartMarker()
{
  assert(m_model);
  ((IntervalModel*)m_model)->setStartMarker(TimeVal::zero());
}

void TimeSignatureItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  if(ossia::any_of(m_handles, [](auto handle) { return handle->pressed; }))
    return;

  QMenu menu;
  {
    auto act = menu.addAction("Add signature change");
    connect(act, &QAction::triggered, this, [this, pos = event->pos()] {
      requestNewHandle(pos);
    });
  }
  {
    auto act = menu.addAction("Set start marker");
    connect(act, &QAction::triggered, this, [this, pos = event->pos()] {
      setStartMarker(pos);
    });
  }
  {
    auto act = menu.addAction("Remove start marker");
    connect(act, &QAction::triggered, this, &TimeSignatureItem::removeStartMarker);
  }

  menu.exec(event->screenPos());
}

}
