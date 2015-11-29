#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>
#include <qlineedit.h>
#include <qspinbox.h>
#include <qstring.h>
#include <qwidget.h>

class NetworkSettingsPresenter;
class QObject;

class NetworkSettingsView : public iscore::SettingsDelegateViewInterface
{
        Q_OBJECT
    public:
        explicit NetworkSettingsView(QObject* parent);

        void setMasterPort(int val);
        void setClientPort(int val);
        void setClientName(QString text);

        QWidget* getWidget() override;
        void load();
        void doConnections();

    public slots:
        void on_masterPortChanged(int);
        void on_clientPortChanged(int);
        void on_clientNameChanged();

    private:
        NetworkSettingsPresenter* presenter();

        QWidget* m_widget {new QWidget};

        QSpinBox* m_masterPort {new QSpinBox{m_widget}};
        QSpinBox* m_clientPort {new QSpinBox{m_widget}};
        QLineEdit* m_clientName {new QLineEdit{m_widget}};

        int m_previousMasterPort {};
        int m_previousClientPort {};
        QString m_previousClientName {};

};
