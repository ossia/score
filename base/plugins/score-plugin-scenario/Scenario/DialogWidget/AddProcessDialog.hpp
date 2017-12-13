#pragma once
#include <QDialog>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <utility>
#include <vector>
#include <set>
#include <QListWidget>

#include <score/plugins/customfactory/UuidKey.hpp>
class QListWidget;

namespace Process
{
class ProcessFactoryList;
class StateProcessList;
class ProcessModelFactory;
class LayerFactory;
class StateProcessFactory;
class ProcessModel;
}
namespace Scenario
{

template<typename FactoryList>
class AddProcessDialog final : public QDialog
{
public:
    using Key = typename FactoryList::key_type;
    class ProcessItem : public QListWidgetItem
    {
      public:
        using QListWidgetItem::QListWidgetItem;
        Key key;
    };

  AddProcessDialog(const FactoryList& plist, QWidget* parent)
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
      if(on_okPressed)
        on_okPressed(static_cast<ProcessItem*>(item)->key);
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, [=] {
      auto item = m_processes->currentItem();
      if(item && on_okPressed)
      {
        on_okPressed(static_cast<ProcessItem*>(item)->key);
      }
      QDialog::accept();
    });

    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    setup();
    hide();
  }

  void launchWindow()
  {
    exec();
  }

  std::function<void(Key)> on_okPressed;

private:
  const FactoryList& m_factoryList;
  QListWidget* m_categories{};
  QListWidget* m_processes{};
  void updateProcesses(const QString& str)
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
  void setup()
  {
    std::set<QString> categories;
    for (const auto& factory : m_factoryList)
    {
      auto cat = factory.category();
      if(!cat.isEmpty())
        categories.insert(std::move(cat));
    }
    for(const auto& str : categories)
    {
      m_categories->addItem(str);
    }
  }
};

}
