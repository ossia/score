#include "StringListEditor.hpp"

#include <QHelpEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace Process
{
namespace
{
class StringListDelegate final : public QStyledItemDelegate
{
public:
  explicit StringListDelegate(QListWidget* rows)
      : QStyledItemDelegate{rows}
      , m_rows{rows}
      , m_icons{
            rows->style()->standardIcon(QStyle::SP_ArrowUp),
            rows->style()->standardIcon(QStyle::SP_ArrowDown),
            rows->style()->standardIcon(QStyle::SP_DialogCloseButton)}
  {
    for(auto& icon : m_icons)
    {
      auto pixmap = icon.pixmap(QSize{16, 16}, rows->devicePixelRatioF());
      {
        QPainter painter{&pixmap};
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), rows->palette().color(QPalette::Text));
      }
      icon = QIcon{pixmap};
    }
    rows->viewport()->installEventFilter(this);
    connect(
        this, &QAbstractItemDelegate::closeEditor, this,
        [this](QWidget* editor, QAbstractItemDelegate::EndEditHint) {
      if(m_editor == editor)
        m_editor.clear();
    });
  }

  std::function<void(int, int)> on_action;

  static constexpr int actionWidth = 20;

  static QRect actionRect(const QRect& row, int action)
  {
    return {
        row.right() + 1 - (3 - action) * actionWidth, row.top(), actionWidth,
        row.height()};
  }

  void paint(
      QPainter* painter, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override
  {
    QStyleOptionViewItem text{option};
    text.rect.setRight(actionRect(option.rect, 0).left() - 1);
    QStyledItemDelegate::paint(painter, text, index);
    QStyleOptionViewItem background{option};
    initStyleOption(&background, index);
    background.text.clear();
    background.rect.setLeft(text.rect.right() + 1);
    m_rows->style()->drawControl(QStyle::CE_ItemViewItem, &background, painter, m_rows);
    for(int action = 0; action < 3; ++action)
      m_icons[action].paint(
          painter, actionRect(option.rect, action).adjusted(3, 3, -3, -3),
          Qt::AlignCenter, enabled(index, action) ? QIcon::Normal : QIcon::Disabled);
  }

  QSize
  sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
  {
    auto size = QStyledItemDelegate::sizeHint(option, index);
    size.setWidth(0);
    size.setHeight(std::max(size.height(), actionWidth));
    return size;
  }

  void updateEditorGeometry(
      QWidget* editor, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override
  {
    auto text = option;
    text.rect.setRight(actionRect(option.rect, 0).left() - 1);
    QStyledItemDelegate::updateEditorGeometry(editor, text, index);
  }

  QWidget* createEditor(
      QWidget* parent, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override
  {
    m_editor = QStyledItemDelegate::createEditor(parent, option, index);
    return m_editor;
  }

  void finishEditing()
  {
    const auto editor = m_editor;
    m_editor.clear();
    if(editor)
    {
      commitData(editor);
      if(editor)
        closeEditor(editor, QAbstractItemDelegate::NoHint);
    }
  }

  bool editorEvent(
      QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option,
      const QModelIndex& index) override
  {
    if(event->type() != QEvent::MouseButtonPress
       && event->type() != QEvent::MouseButtonRelease
       && event->type() != QEvent::MouseButtonDblClick)
      return false;
    auto mouse = static_cast<QMouseEvent*>(event);
    if(mouse->button() != Qt::LeftButton)
      return false;
    for(int action = 0; action < 3; ++action)
    {
      if(!actionRect(option.rect, action).contains(mouse->position().toPoint()))
        continue;
      if(event->type() == QEvent::MouseButtonPress)
      {
        m_pressedKey = index.data(Qt::UserRole);
        m_pressedAction = action;
      }
      else if(event->type() == QEvent::MouseButtonRelease)
      {
        const auto key = index.data(Qt::UserRole);
        const bool activate
            = key == m_pressedKey && action == m_pressedAction && enabled(index, action);
        m_pressedKey.clear();
        m_pressedAction = -1;
        if(activate)
        {
          finishEditing();
          if(on_action)
            on_action(key.toInt(), action);
        }
      }
      return true;
    }
    m_pressedKey.clear();
    m_pressedAction = -1;
    return false;
  }

  bool helpEvent(
      QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
      const QModelIndex& index) override
  {
    for(int action = 0; action < 3; ++action)
      if(actionRect(option.rect, action).contains(event->pos()))
      {
        static const char* labels[]{"Move up", "Move down", "Delete row"};
        QToolTip::showText(event->globalPos(), tr(labels[action]), view);
        return true;
      }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
  }

protected:
  bool eventFilter(QObject* object, QEvent* event) override
  {
    if(object == m_rows->viewport()
       && (event->type() == QEvent::MouseButtonPress
           || event->type() == QEvent::MouseButtonRelease
           || event->type() == QEvent::MouseButtonDblClick))
    {
      auto mouse = static_cast<QMouseEvent*>(event);
      const auto index = m_rows->indexAt(mouse->position().toPoint());
      QStyleOptionViewItem option;
      option.rect = m_rows->visualRect(index);
      if(index.isValid() && editorEvent(event, m_rows->model(), option, index))
        return true;
    }
    // Match the inline combo editor: claim commit/cancel keys before the
    // document's transport shortcuts, then let the delegate handle the press.
    if(event->type() == QEvent::ShortcutOverride)
    {
      const int key = static_cast<QKeyEvent*>(event)->key();
      if(key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Escape)
      {
        event->accept();
        return true;
      }
    }
    return QStyledItemDelegate::eventFilter(object, event);
  }

private:
  bool enabled(const QModelIndex& index, int action) const
  {
    return action == 2
           || (action == 0 ? index.row() > 0 : index.row() + 1 < m_rows->count());
  }

  QListWidget* m_rows{};
  QIcon m_icons[3];
  mutable QPointer<QWidget> m_editor;
  QVariant m_pressedKey;
  int m_pressedAction{-1};
};
}

StringListEditor::StringListEditor(QWidget* parent)
    : QWidget{parent}
{
  setObjectName("StringListEditor");
  auto layout = new QVBoxLayout{this};
  layout->setContentsMargins(0, 0, 0, 0);
  rows = new QListWidget{this};
  rows->setObjectName("StringListRows");
  auto delegate = new StringListDelegate{rows};
  rows->setItemDelegate(delegate);
  rows->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  delegate->on_action = [this](int key, int action) {
    for(int row = 0; row < rows->count(); ++row)
    {
      if(rows->item(row)->data(Qt::UserRole).toInt() != key)
        continue;
      if(action == 2)
      {
        delete rows->takeItem(row);
        commit();
      }
      else
        move(row, action == 0 ? -1 : 1);
      break;
    }
  };
  rows->setEditTriggers(
      QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
  layout->addWidget(rows);
  auto add = new QPushButton{"+", this};
  add->setObjectName("StringListAdd");
  add->setToolTip(tr("Add row"));
  add->setAccessibleName(tr("Add row"));
  add->setFixedWidth(28);
  layout->addWidget(add, 0, Qt::AlignLeft);
  connect(add, &QPushButton::clicked, this, [this, delegate] {
    delegate->finishEditing();
    if(rows->count() >= 512)
      return;
    int key = 10000;
    for(int i = 0; i < rows->count(); ++i)
    {
      const int id = rows->item(i)->data(Qt::UserRole).toInt();
      if(id == std::numeric_limits<int>::max())
        return;
      key = std::max(key, id + 1);
    }
    {
      QSignalBlocker block{rows};
      append(key, {});
      rows->setCurrentRow(rows->count() - 1);
    }
    commit();
  });
  connect(rows, &QListWidget::itemChanged, this, [this] { commit(); });
  setMinimumSize(120, 180);
  resize(260, 180);
}

void StringListEditor::setValue(const ossia::value& value)
{
  if(this->value() == value)
    return;
  QSignalBlocker block{rows};
  const auto selected
      = rows->currentItem() ? rows->currentItem()->data(Qt::UserRole) : QVariant{};
  rows->clear();
  if(auto list = value.target<std::vector<ossia::value>>())
    for(const auto& entry : *list)
      if(auto pair = entry.target<std::vector<ossia::value>>();
         pair && pair->size() == 2)
        if(auto key = (*pair)[0].target<int>())
          if(auto text = (*pair)[1].target<std::string>())
          {
            append(*key, QString::fromStdString(*text));
            if(rows->item(rows->count() - 1)->data(Qt::UserRole) == selected)
              rows->setCurrentRow(rows->count() - 1);
          }
}

ossia::value StringListEditor::value() const
{
  std::vector<ossia::value> value;
  value.reserve(rows->count());
  for(int i = 0; i < rows->count(); ++i)
  {
    const auto item = rows->item(i);
    value.emplace_back(
        std::vector<ossia::value>{
            item->data(Qt::UserRole).toInt(), item->text().toStdString()});
  }
  return value;
}

void StringListEditor::append(int key, const QString& text)
{
  auto item = new QListWidgetItem{text, rows};
  item->setData(Qt::UserRole, key);
  item->setFlags(item->flags() | Qt::ItemIsEditable);
}

void StringListEditor::commit()
{
  if(on_edited)
    on_edited(value());
}

void StringListEditor::move(int from, int direction)
{
  const int to = from + direction;
  if(from < 0 || to < 0 || to >= rows->count())
    return;
  auto item = rows->takeItem(from);
  rows->insertItem(to, item);
  rows->setCurrentRow(to);
  commit();
}
}
