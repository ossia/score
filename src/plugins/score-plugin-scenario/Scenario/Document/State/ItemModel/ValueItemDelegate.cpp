#include "ValueItemDelegate.hpp"

#include <Explorer/Explorer/ValueEditors.hpp>

#include <State/Widgets/Values/ExpandableTextEdit.hpp>

#include <Device/Address/AddressSettings.hpp>

#include <ossia-qt/metatypes.hpp>

#include <algorithm>
#include <memory>

namespace Scenario
{
ValueItemDelegate::ValueItemDelegate(int valueColumn, QObject* parent)
    : QStyledItemDelegate{parent}
    , m_column{valueColumn}
{
}

ValueItemDelegate::~ValueItemDelegate() = default;

QWidget* ValueItemDelegate::createEditor(
    QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  if(index.column() == m_column)
  {
    const auto v = index.data(OssiaValueRole).value<ossia::value>();
    if(v.valid())
    {
      // A state holds a value, not a parameter: no domain, no unit, only type.
      Device::AddressSettingsCommon as;
      as.value = v;

      if(auto* w = Explorer::make_value_widget(
             as, parent, Explorer::ValueEditorSize::Compact))
      {
        auto* self = const_cast<ValueItemDelegate*>(this);
        self->m_live.insert(w);
        connect(w, &QObject::destroyed, self, [self, w] { self->m_live.remove(w); });

        if(w->commitsImmediately())
        {
          connect(
              w, &Explorer::AddressValueWidget::changed, self,
              [self, w](const ossia::value&) {
            if(self->owns(w))
              self->commitData(w);
              });
        }

        // Clicking away is the editor's own business: see the focus-out in
        // AddressValueWidget::eventFilter.
        return w;
      }
    }
  }

  return QStyledItemDelegate::createEditor(parent, option, index);
}

void ValueItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
  if(auto* w = qobject_cast<Explorer::AddressValueWidget*>(editor))
  {
    if(const auto v = index.data(OssiaValueRole).value<ossia::value>(); v.valid())
    {
      w->set(v);
      if(auto* t = w->findChild<State::ExpandableTextEdit*>())
        t->expandIfNeeded();
      return;
    }
  }

  QStyledItemDelegate::setEditorData(editor, index);
}

void ValueItemDelegate::destroyEditor(QWidget* editor, const QModelIndex& index) const
{
  m_live.remove(editor);
  QStyledItemDelegate::destroyEditor(editor, index);
}

void ValueItemDelegate::setModelData(
    QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
  if(auto* w = qobject_cast<Explorer::AddressValueWidget*>(editor))
  {
    if(!w->edited())
      return;

    if(auto v = w->get(); v.valid())
      model->setData(index, QVariant::fromValue(v), Qt::EditRole);
    return;
  }

  QStyledItemDelegate::setModelData(editor, model, index);
}

void ValueItemDelegate::updateEditorGeometry(
    QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  // Exactly the cell, as in DeviceExplorerDelegate.
  Explorer::fitEditorToCell(*editor, option.rect);
}
}
