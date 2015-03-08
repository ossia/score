#pragma once
#include <set>
#include <memory>
#include <plugin_interface/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

#include <QDialog>
#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QStackedWidget>
namespace iscore
{
    class SettingsView : public QDialog
    {
            Q_OBJECT
        public:
            SettingsView(QWidget* parent);
            void addSettingsView(SettingsDelegateViewInterface* view);

        private:
            std::set<SettingsDelegateViewInterface*> m_pluginViews;

            QVBoxLayout* m_vertLayout {new QVBoxLayout{}};
            QHBoxLayout* m_hboxLayout {new QHBoxLayout{}};
            QListWidget* m_settingsList {new QListWidget{this}};
            QStackedWidget* m_stackedWidget {new QStackedWidget{this}};

            QDialogButtonBox* m_buttons {new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                        this
                                                                 }
            };

    };
}
