#pragma once
#include <qboxlayout.h>
#include <qdialog.h>
#include <qdialogbuttonbox.h>
#include <qflags.h>
#include <qlistwidget.h>
#include <qstackedwidget.h>
#include <set>

class QWidget;
namespace iscore {
class SettingsDelegateViewInterface;
}  // namespace iscore

namespace iscore
{
    class SettingsView final : public QDialog
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
