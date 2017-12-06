// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessList.hpp>
#include <QApplication>
#include <QInputDialog>
#include <QListWidget>

#include <QString>
#include <QStringList>
#include <algorithm>
#include <utility>
#include <vector>
#include <set>

#include "AddProcessDialog.hpp"
#include <Process/ProcessFactory.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <QHBoxLayout>
#include <QDialogButtonBox>
namespace Scenario
{
class ProcessItem : public QListWidgetItem
{
  public:
    using QListWidgetItem::QListWidgetItem;
    UuidKey<Process::ProcessModel> key;
};
AddProcessDialog::AddProcessDialog(
    const Process::ProcessFactoryList& plist, QWidget* parent)
    : QDialog{parent}, m_factoryList{plist}
{
  auto lay = new QHBoxLayout;
  this->setLayout(lay);
  m_categories = new QListWidget;
  lay->addWidget(m_categories);
  m_processes = new QListWidget;
  lay->addWidget(m_processes);
  auto buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
  buttonBox->setOrientation(Qt::Vertical);
  lay->addWidget(buttonBox);

  connect(m_categories, &QListWidget::currentTextChanged,
          this, &AddProcessDialog::updateProcesses);

  connect(m_processes, &QListWidget::itemDoubleClicked,
          this, [&] (auto item) {
    emit okPressed(static_cast<ProcessItem*>(item)->key);
  });

  connect(buttonBox, &QDialogButtonBox::accepted, this, [=] {
    auto item = m_processes->currentItem();
    if(item)
    {
      emit okPressed(static_cast<ProcessItem*>(item)->key);
    }
    QDialog::accept();
  });

  connect(buttonBox, &QDialogButtonBox::rejected,
          this, &QDialog::reject);
  setup();
  hide();
}

void AddProcessDialog::updateProcesses(const QString& str)
{
  m_processes->clear();
  for (const auto& factory : m_factoryList)
  {
    if(factory.category() == str)
    {
      auto item = new ProcessItem{factory.prettyName()};
      item->key = factory.concreteKey();
      m_processes->addItem(item);
    }
  }
}

void AddProcessDialog::setup()
{
  std::set<QString> categories;
  for (const auto& factory : m_factoryList)
  {
    categories.insert(factory.category());
  }
  for(const auto& str : categories)
  {
    m_categories->addItem(str);
  }
}

void AddProcessDialog::launchWindow()
{
  exec();
}

AddStateProcessDialog::AddStateProcessDialog(
    const Process::StateProcessList& plist, QWidget* parent)
    : QWidget{parent}, m_factoryList{plist}
{
  hide();
}

void AddStateProcessDialog::launchWindow()
{
  bool ok = false;

  std::vector<std::pair<QString, UuidKey<Process::StateProcessFactory>>>
      sortedFactoryList;
  for (const auto& factory : m_factoryList)
  {
    sortedFactoryList.push_back(
        std::make_pair(factory.prettyName(), factory.concreteKey()));
  }

  std::sort(
      sortedFactoryList.begin(),
      sortedFactoryList.end(),
      [](const auto& e1, const auto& e2) { return e1.first < e2.first; });

  QStringList nameList;
  for (const auto& elt : sortedFactoryList)
  {
    nameList.append(elt.first);
  }

  auto process_name = QInputDialog::getItem(
      qApp->activeWindow(),
      tr("Choose a state process"),
      tr("Choose a state process"),
      nameList,
      0,
      false,
      &ok);

  if (ok)
  {
    auto it = std::find_if(
        sortedFactoryList.begin(),
        sortedFactoryList.end(),
        [&](const auto& elt) { return elt.first == process_name; });
    SCORE_ASSERT(it != sortedFactoryList.end());
    emit okPressed(it->second);
  }
}
}
