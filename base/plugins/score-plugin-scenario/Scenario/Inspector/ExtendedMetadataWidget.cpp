// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExtendedMetadataWidget.hpp"

#include <score/tools/QMapHelper.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QAction>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace Scenario
{

ExtendedMetadataWidget::ExtendedMetadataWidget(
    const QVariantMap& map, QWidget* parent)
    : QWidget{parent}
{
  m_layout = new score::MarginLess<QFormLayout>(this);

  m_addButton = new QPushButton{tr("Add metadata")};
  connect(
      m_addButton, &QPushButton::clicked, this,
      [=]() {
        QString txt = QInputDialog::getText(
            this, tr("Property name?"),
            tr("Enter a name for the new property:"));
        if (!txt.isEmpty())
        {
          insertRow(txt, m_widgets.size());
          dataChanged();
        }
      },
      Qt::QueuedConnection);
  m_layout->addWidget(m_addButton);

  setup(map);
}

void ExtendedMetadataWidget::update(const QVariantMap& map)
{
  // OPTIMIZEME
  setup(map);
}

void ExtendedMetadataWidget::setup(const QVariantMap& map)
{
  for (auto item : m_widgets)
  {
    item.first->deleteLater();
    item.second->deleteLater();
  }
  m_widgets.clear();
  setup_impl(map);
}

QVariantMap ExtendedMetadataWidget::currentMap() const
{
  QVariantMap map;
  for (auto elt : m_widgets)
  {
    map.insert(elt.first->text(), elt.second->text());
  }
  return map;
}

std::pair<QLabel*, QLineEdit*>
ExtendedMetadataWidget::makeRow(const QString& l, const QString& r, int row)
{
  auto left = new TextLabel{l};
  left->setContextMenuPolicy(Qt::ActionsContextMenu);
  auto remove_act = new QAction{tr("Remove"), left};
  connect(
      remove_act, &QAction::triggered, this, [=] { on_rowRemoved(row); },
      Qt::QueuedConnection);
  left->addAction(remove_act);

  auto right = new QLineEdit{r};
  connect(
      right, &QLineEdit::editingFinished, this, [=] { on_rowChanged(row); },
      Qt::QueuedConnection);

  return std::make_pair(left, right);
}

void ExtendedMetadataWidget::insertRow(const QString& label, int row)
{
  auto res = makeRow(label, "", row);
  m_layout->insertRow(row, res.first, res.second);
  m_widgets.push_back(res);
}

void ExtendedMetadataWidget::on_rowChanged(int i)
{
  // OPTIMIZEME
  dataChanged();
}

void ExtendedMetadataWidget::on_rowRemoved(int i)
{
  // OPTIMIZEME
  if (i < (int)m_widgets.size())
  {
    auto& items = m_widgets[i];
    items.first->deleteLater();
    items.second->deleteLater();
    m_widgets.erase(m_widgets.begin() + i);
  }
  dataChanged();
}

void ExtendedMetadataWidget::setup_impl(const QVariantMap& map)
{
  int i = 0;
  for (auto& k : QMap_keys(map))
  {
    auto row = makeRow(k, map[k].toString(), i);
    m_layout->insertRow(0, row.first, row.second);
    m_widgets.push_back(row);
    ++i;
  }
}
}
