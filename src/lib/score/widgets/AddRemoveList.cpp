#include "AddRemoveList.hpp"

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::AddRemoveList)

namespace score
{

AddRemoveList::AddRemoveList(const QString& root, const QStringList& data, QWidget* parent)
  : QListWidget{parent}
  , m_root{root}
{
  setAlternatingRowColors(true);
  setEditTriggers(QListWidget::DoubleClicked);

  replaceContent(data);
  connect(this, &AddRemoveList::itemChanged, this, [this](auto item) {
    if (m_editing != 0)
      return;
    on_itemChanged(item);
    changed();
  });
}

void AddRemoveList::on_itemChanged(QListWidgetItem* item)
{
  m_editing++;
start:
  for (int i = 0; i < count(); i++)
  {
    auto other = this->item(i);
    if (other != item)
    {
      if (other->text() == item->text())
      {
        item->setText(item->text() + "_");
        goto start;
      }
    }
  }
  m_editing--;
}

void AddRemoveList::fix(int k)
{
  m_editing++;
  item(k)->setText(item(k)->text() + "_");
  on_itemChanged(item(k));
  m_editing--;
}

void AddRemoveList::replaceContent(const QStringList& values)
{
  clear();
  for (auto& str : values)
  {
    auto item = new QListWidgetItem{str};
    item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
    addItem(item);
  }
}

QStringList AddRemoveList::content() const noexcept
{
  QStringList c;
  const int n = count();
  c.reserve(n);
  for (int i = 0; i < n; i++)
    c.push_back(item(i)->text());
  return c;
}

bool AddRemoveList::sameContent(const QStringList& values)
{
  const int n = count();
  if (n != values.size())
    return false;
  for (int i = 0; i < n; i++)
    if (item(i)->text() != values[i])
      return false;
  return true;
}

void AddRemoveList::on_add(const QString& name)
{
  auto item = new QListWidgetItem{name};
  item->setFlags(item->flags() | Qt::ItemFlag::ItemIsEditable);
  addItem(item);
  editItem(item);
  changed();
}

void AddRemoveList::on_remove()
{
  const auto& selection = selectedItems();
  for (auto item : selection)
  {
    removeItemWidget(item);
  }
  changed();
}

void AddRemoveList::setCount(int i)
{
  while (count() < i)
  {
    on_add(m_root + QString::number(count()));
  }
  while (count() > i)
  {
    delete takeItem(count() - 1);
  }
}

void AddRemoveList::sanitize(AddRemoveList* changed, const AddRemoveList* other)
{
  bool must_recheck{};
  auto c1 = changed->content();
  auto c2 = other->content();
  int k = 0;
  for (auto& e1 : c1)
  {
    for (auto& e2 : c2)
    {
      if (e1 == e2)
      {
        changed->fix(k);
        must_recheck = true;
        break;
      }
    }
    k++;
  }

  if (must_recheck)
    sanitize(changed, other);
}

}
