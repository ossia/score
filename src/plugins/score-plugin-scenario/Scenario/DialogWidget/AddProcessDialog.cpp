#include "AddProcessDialog.hpp"

#include <QGridLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

#include <set>
namespace Scenario
{
class ProcessItem : public QListWidgetItem
{
public:
  using QListWidgetItem::QListWidgetItem;
  AddProcessDialog::Key key;
};

AddProcessDialog::AddProcessDialog(
    const Process::ProcessFactoryList& plist,
    Process::ProcessFlags acceptable,
    QWidget* parent)
    : QDialog{parent}, m_factoryList{plist}, m_flags{acceptable}
{
  setWindowTitle(tr("Add process"));
  auto lay = new QGridLayout;
  lay->setSpacing(4);
  this->setLayout(lay);

  auto categoriesLabel = new QLabel{tr("Categories"),this};
  auto titleFont = categoriesLabel->font();
  titleFont.setBold(true);
  titleFont.setPixelSize(14);
  categoriesLabel->setFont(titleFont);
  categoriesLabel->setAlignment(Qt::AlignHCenter);
  lay->addWidget(categoriesLabel,0,0);
  m_categories = new QListWidget;
  lay->addWidget(m_categories,1,0,-1,1);

  auto processLabel = new QLabel{tr("Process"),this};
  processLabel->setFont(titleFont);
  processLabel->setAlignment(Qt::AlignHCenter);
  lay->addWidget(processLabel,0,1);

  m_processes = new QListWidget;
  lay->addWidget(m_processes,1,1,-1,1);

  auto add = new QPushButton{"+", this};
  auto btnFont = add->font();
  btnFont.setPixelSize(28);
  add->setFont(btnFont);
  add->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  lay->addWidget(add,1,2,-1,1);

  connect(
      m_categories, &QListWidget::currentTextChanged, this, &AddProcessDialog::updateProcesses);

  auto accept_item = [&](auto item) {
    if (item && on_okPressed)
    {
      auto k = static_cast<ProcessItem*>(item)->key;
      auto fact = m_factoryList.get(k);
      QString dat;
      if ((int)fact->flags() & (int)Process::ProcessFlags::RequiresCustomData)
      {
        dat = fact->customConstructionData();
        if (!dat.isEmpty())
          on_okPressed(k, dat);
      }
      else
      {
        on_okPressed(k, dat);
      }
    }
  };
  connect(m_processes, &QListWidget::itemDoubleClicked, this, accept_item);
  connect(add, &QPushButton::clicked, [=] { accept_item(m_processes->currentItem()); });

  setup();
  hide();
}

AddProcessDialog::~AddProcessDialog() { }

void AddProcessDialog::launchWindow()
{
  exec();
}

void AddProcessDialog::updateProcesses(const QString& str)
{
  m_processes->clear();
  for (const auto& factory : m_factoryList)
  {
    if (factory.category() == str && ((int)factory.flags() & (int)m_flags))
    {
      auto item = new ProcessItem{factory.prettyName()};
      item->key = factory.concreteKey();
      m_processes->addItem(item);
    }
  }
}

void AddProcessDialog::setup()
{
  std::set<QString, std::less<>> categories;
  for (const auto& factory : m_factoryList)
  {
    auto cat = factory.category();
    if (!cat.isEmpty() && ((int)factory.flags() & (int)m_flags))
      categories.insert(std::move(cat));
  }
  for (const auto& str : categories)
  {
    auto item = new ProcessItem{Process::getCategoryIcon(str), str};
    m_categories->addItem(item);
  }
}
}
