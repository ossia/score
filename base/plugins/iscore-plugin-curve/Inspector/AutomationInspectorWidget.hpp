#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class AutomationModel;
class AutomationInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit AutomationInspectorWidget(
                const AutomationModel* object,
                QWidget* parent);

    public slots:
        void on_addressChange(const QString& newText);

    signals:
        void createViewInNewDeck(QString);

    private:
        QLineEdit* m_lineEdit{};
        const AutomationModel* m_model {};
};
