#pragma once

#include <InspectorInterface/InspectorWidgetBase.hpp>

class AutomationModel;
class AutomationInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit AutomationInspectorWidget(AutomationModel* object,
                                           QWidget* parent = 0);

    public slots:
        void on_addressChange(const QString& newText);

    signals:
        void createViewInNewDeck(QString);

    private:
        QLineEdit* m_lineEdit {};
        AutomationModel* m_model {};
};
