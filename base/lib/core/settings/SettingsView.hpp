#pragma once
#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFlags>
#include <QListWidget>
#include <QSize>
#include <QStackedWidget>
#include <QWidget>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <set>

class QWidget;

namespace score
{
template <typename Model>
class SettingsView final : public QDialog
{
public:
  SettingsView(QWidget* parent) : QDialog{parent}
  {
    {
      auto centerWidg = new QWidget{this};
      {
        m_hboxLayout->addWidget(m_settingsList);
        m_hboxLayout->addWidget(m_stackedWidget);
        centerWidg->setLayout(m_hboxLayout);
      }

      m_vertLayout->addWidget(centerWidg);
      m_vertLayout->addWidget(m_buttons);
      this->setLayout(m_vertLayout);
    }

    connect(
        m_settingsList, &QListWidget::currentRowChanged, m_stackedWidget,
        &QStackedWidget::setCurrentIndex);

    connect(
        m_buttons, &QDialogButtonBox::accepted, this, &SettingsView::accept);
    connect(
        m_buttons, &QDialogButtonBox::rejected, this, &SettingsView::reject);
  }
  void addSettingsView(SettingsDelegateView<Model>* view)
  {
    view->setParent(this);
    QListWidgetItem* it = new QListWidgetItem{
        view->getPresenter()->settingsIcon(),
        view->getPresenter()->settingsName(), m_settingsList};
    it->setSizeHint(QSize{0, 30});
    m_settingsList->addItem(it);
    m_stackedWidget->addWidget(view->getWidget());

    m_pluginViews.insert(view);
  }

private:
  std::set<SettingsDelegateView<Model>*> m_pluginViews;

  QVBoxLayout* m_vertLayout{new QVBoxLayout{}};
  QHBoxLayout* m_hboxLayout{new QHBoxLayout{}};
  QListWidget* m_settingsList{new QListWidget{this}};
  QStackedWidget* m_stackedWidget{new QStackedWidget{this}};

  QDialogButtonBox* m_buttons{new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this}};
};
}
