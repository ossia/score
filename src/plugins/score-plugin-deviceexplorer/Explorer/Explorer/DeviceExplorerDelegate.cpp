#include "DeviceExplorerDelegate.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <Explorer/Explorer/Column.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/ValueEditors.hpp>

#include <State/Widgets/Values/ExpandableTextEdit.hpp>

#include <QAbstractProxyModel>
#include <score/graphics/BangPainting.hpp>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace Explorer
{
namespace
{
//! An impulse has no value to show, only something to press.
bool isImpulse(const Device::AddressSettings* addr) noexcept
{
  return addr && addr->value.get_type() == ossia::val_type::IMPULSE;
}
}

DeviceExplorerDelegate::DeviceExplorerDelegate(QObject* parent)
    : QStyledItemDelegate{parent}
{
}

DeviceExplorerDelegate::~DeviceExplorerDelegate() = default;

//! The view may be behind the search filter; the delegate keys everything it
//! remembers on the tree's own indices.
QModelIndex DeviceExplorerDelegate::sourceIndex(const QModelIndex& index) noexcept
{
  QModelIndex idx = index;
  while(auto* proxy = qobject_cast<const QAbstractProxyModel*>(idx.model()))
    idx = proxy->mapToSource(idx);
  return idx;
}

const Device::AddressSettings*
DeviceExplorerDelegate::addressAt(const QModelIndex& index) noexcept
{
  const QModelIndex idx = sourceIndex(index);

  if(!qobject_cast<const DeviceExplorerModel*>(idx.model()))
    return nullptr;
  if(!idx.isValid() || idx.column() != (int)Column::Value)
    return nullptr;

  auto* node = static_cast<Device::Node*>(idx.internalPointer());
  if(!node || !node->is<Device::AddressSettings>())
    return nullptr;

  return &node->get<Device::AddressSettings>();
}

QWidget* DeviceExplorerDelegate::createEditor(
    QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  if(auto* addr = addressAt(index))
  {
    // The bang is painted in the cell and answers the first click; an editor
    // would drop a second, identical button on top of it.
    if(isImpulse(addr))
      return nullptr;

    if(auto* w = make_value_widget(*addr, parent, ValueEditorSize::Compact))
    {
      auto* self = const_cast<DeviceExplorerDelegate*>(this);
      m_live.insert(w);
      connect(w, &QObject::destroyed, self, [self, w] { self->m_live.remove(w); });

      if(w->commitsImmediately())
      {
        connect(w, &AddressValueWidget::changed, self, [self, w](const ossia::value&) {
          if(self->owns(w))
            self->commitData(w);
        });
      }

      // Clicking away is the editor's own business: AddressValueWidget's
      // focus-out hands the event to Qt's editor filter.
      return w;
    }
  }

  return QStyledItemDelegate::createEditor(parent, option, index);
}

void DeviceExplorerDelegate::setEditorData(
    QWidget* editor, const QModelIndex& index) const
{
  if(auto* w = qobject_cast<AddressValueWidget*>(editor))
  {
    if(auto* addr = addressAt(index))
    {
      w->set(addr->value);

      // Several lines, or bytes: the one-line field cannot show it, so open
      // the popup straight away rather than a preview nobody can type in.
      if(auto* t = w->findChild<State::ExpandableTextEdit*>())
        t->expandIfNeeded();
      return;
    }
  }

  QStyledItemDelegate::setEditorData(editor, index);
}

void DeviceExplorerDelegate::destroyEditor(
    QWidget* editor, const QModelIndex& index) const
{
  m_live.remove(editor);
  QStyledItemDelegate::destroyEditor(editor, index);
}

void DeviceExplorerDelegate::setModelData(
    QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
  if(auto* w = qobject_cast<AddressValueWidget*>(editor))
  {
    if(!w->edited())
      return;

    if(auto v = w->get(); v.valid())
      model->setData(index, QVariant::fromValue(v), Qt::EditRole);
    return;
  }

  QStyledItemDelegate::setModelData(editor, model, index);
}

namespace
{
//! score::QGraphicsButton's circle, sized to the row.
QRectF bangCircle(const QStyleOptionViewItem& option)
{
  const qreal side = std::max(8.0, std::min<qreal>(option.rect.height() - 4, 14));
  return QRectF{
      option.rect.x() + 2.,
      option.rect.y() + (option.rect.height() - side) / 2., side, side};
}

//! A little larger than the circle, so it can be hit without aiming.
QRect bangHitArea(const QStyleOptionViewItem& option)
{
  return bangCircle(option).toRect().adjusted(-2, -2, 2, 2);
}

//! Where the cells are painted: option.widget is the view, but the items live
//! on its viewport, and repainting the parent leaves the child alone.
QWidget* paintSurface(const QStyleOptionViewItem& option)
{
  auto* w = const_cast<QWidget*>(option.widget);
  if(auto* area = qobject_cast<QAbstractScrollArea*>(w))
    return area->viewport();
  return w;
}

void drawBang(QPainter& p, const QStyleOptionViewItem& option, bool lit)
{
  const auto circle = bangCircle(option);

  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);

  p.setPen(Qt::NoPen);
  p.setBrush(score::bangFill(option.palette, lit));
  p.drawEllipse(circle);

  if(lit)
  {
    // Pressed: the fill drops back and a ring is drawn inside it.
    const qreal inset = circle.width() * 0.125;
    p.setPen(QPen{score::bangFill(option.palette, false).color(), 1.5});
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(circle.adjusted(inset, inset, -inset, -inset));
  }

  p.restore();
}
}

void DeviceExplorerDelegate::watch(const QAbstractItemModel* model) const
{
  // paint() calls this for every cell it draws; the answer only changes when
  // the view is given a different model.
  if(!model || m_seen == model)
    return;
  m_seen = model;

  // The traffic is reported by the tree, which may sit behind the filter.
  const QAbstractItemModel* src = model;
  while(auto* proxy = qobject_cast<const QAbstractProxyModel*>(src))
    src = proxy->sourceModel();

  auto* tree = qobject_cast<const DeviceExplorerModel*>(src);
  if(!tree || m_watched == tree)
    return;

  // A view swapped back to a model already watched would otherwise flash it
  // twice per impulse.
  QObject::disconnect(m_traffic);
  m_watched = tree;
  auto* self = const_cast<DeviceExplorerDelegate*>(this);

  // valueUpdated, not dataChanged: the latter also fires when an address's
  // settings are replaced, which opening a document does for every address.
  m_traffic = connect(
      tree, &DeviceExplorerModel::valueUpdated, self, [self, tree](Device::Node* n) {
    if(!n || !n->is<Device::AddressSettings>())
      return;
    if(n->get<Device::AddressSettings>().value.get_type() != ossia::val_type::IMPULSE)
      return;

    self->flash(tree->modelIndexFromNode(*n, (int)Column::Value));
  });
}

void DeviceExplorerDelegate::flash(const QModelIndex& index)
{
  const QPersistentModelIndex p{index};
  if(!m_lit.contains(p))
    m_lit.push_back(p);

  if(m_surface)
    m_surface->update();

  // One timer for the delegate, restarted, rather than one per impulse: at OSC
  // rates that was thousands of timers a second, and the first of them to fire
  // put every row out again.
  if(!m_unlit)
  {
    m_unlit = new QTimer{const_cast<DeviceExplorerDelegate*>(this)};
    m_unlit->setSingleShot(true);
    m_unlit->setInterval(90);
    connect(m_unlit, &QTimer::timeout, this, [this] {
      m_lit.clear();
      if(m_surface)
        m_surface->update();
    });
  }
  m_unlit->start();
}

void DeviceExplorerDelegate::paint(
    QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
  watch(index.model());
  m_surface = paintSurface(option);

  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);

  if(isImpulse(addressAt(index)))
  {
    opt.text.clear();
    auto* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const auto src = sourceIndex(index);
    drawBang(*painter, option, m_pressed == src || m_lit.contains(src));
    return;
  }

  if(paintValueWithMarker(*painter, opt, opt.text))
    return;

  QStyledItemDelegate::paint(painter, option, index);
}

bool DeviceExplorerDelegate::editorEvent(
    QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
    const QModelIndex& index)
{
  if(isImpulse(addressAt(index)))
  {
    auto* me = dynamic_cast<QMouseEvent*>(event);
    const bool onBang = me && me->button() == Qt::LeftButton
                        && bangHitArea(option).contains(me->position().toPoint());

    if(onBang && event->type() == QEvent::MouseButtonPress)
    {
      m_pressed = sourceIndex(index);
      if(auto* surface = paintSurface(option))
        surface->update();
      return true;
    }

    if(event->type() == QEvent::MouseButtonRelease)
    {
      // A button pressed on one bang and released on another is not a press of
      // either, as for any other button.
      const auto src = sourceIndex(index);
      const bool armed = onBang && m_pressed == src;
      m_pressed = QModelIndex{};

      if(auto* surface = paintSurface(option))
        surface->update();

      if(armed)
      {
        // Flash here too: the model does not always report the write back.
        model->setData(
            index, QVariant::fromValue(ossia::value{ossia::impulse{}}), Qt::EditRole);
        flash(src);
        return true;
      }

      if(onBang)
        return true;
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void DeviceExplorerDelegate::updateEditorGeometry(
    QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  // The base puts the editor on the item text rect, which leaves the icon and
  // the check indicator of a Name cell alone.
  QStyledItemDelegate::updateEditorGeometry(editor, option, index);

  // Exactly that rect, then: an editor grown to its size hint paints over the
  // rows above and below it.
  if(editor)
    fitEditorToCell(*editor, editor->geometry());
}
}
