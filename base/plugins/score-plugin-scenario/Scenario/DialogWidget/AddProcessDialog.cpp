#include "AddProcessDialog.hpp"
namespace Scenario
{
class ProcessItem : public QListWidgetItem
{
  public:
    using QListWidgetItem::QListWidgetItem;
    AddProcessDialog::Key key;
};

AddProcessDialog::AddProcessDialog(const Process::ProcessFactoryList& plist, Process::ProcessFlags acceptable, QWidget* parent)
  : QDialog{parent}
  , m_factoryList{plist}
  , m_flags{acceptable}
{
  auto lay = new QHBoxLayout;
  this->setLayout(lay);
  m_categories = new QListWidget;
  lay->addWidget(m_categories);
  m_processes = new QListWidget;
  lay->addWidget(m_processes);
  auto buttonBox = new QDialogButtonBox{};
  auto add = new QPushButton{"+"};
  buttonBox->addButton(add, QDialogButtonBox::ActionRole);
  buttonBox->addButton(QDialogButtonBox::StandardButton::Close);
  buttonBox->setOrientation(Qt::Vertical);
  lay->addWidget(buttonBox);

  connect(m_categories, &QListWidget::currentTextChanged,
          this, &AddProcessDialog::updateProcesses);

  auto accept_item = [&] (auto item) {

    if(item && on_okPressed)
    {
      auto k = static_cast<ProcessItem*>(item)->key;
      auto fact = m_factoryList.get(k);
      QString dat;
      if((int)fact->flags() & (int)Process::ProcessFlags::RequiresCustomData)
      {
        dat = fact->customConstructionData();
      }
      on_okPressed(k, dat);
    }
  };
  connect(m_processes, &QListWidget::itemDoubleClicked,
          this, accept_item);
  connect(add, &QPushButton::clicked, [=] {
    accept_item(m_processes->currentItem());
  });

  connect(buttonBox, &QDialogButtonBox::accepted, this, [=] {
    accept_item(m_processes->currentItem());
    QDialog::close();
  });

  connect(buttonBox, &QDialogButtonBox::rejected,
          this, &QDialog::reject);
  setup();
  hide();
}

void AddProcessDialog::launchWindow()
{
  exec();
}

void AddProcessDialog::updateProcesses(const QString& str)
{
  m_processes->clear();
  for (const auto& factory : m_factoryList)
  {
    if(factory.category() == str && ((int)factory.flags() & (int)m_flags))
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
    auto cat = factory.category();
    if(!cat.isEmpty() && ((int)factory.flags() & (int)m_flags))
      categories.insert(std::move(cat));
  }
  for(const auto& str : categories)
  {
    m_categories->addItem(str);
  }
}

}
