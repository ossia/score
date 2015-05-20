#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class AutomationModel;
class QDoubleSpinBox;
class AutomationInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit AutomationInspectorWidget(
                const AutomationModel* object,
                QWidget* parent);

    signals:
        void createViewInNewDeck(QString);

    public slots:
        void on_addressChange(const QString& newText);
        void on_minValueChanged();
        void on_maxValueChanged();

    private:
        QLineEdit* m_lineEdit{};
        QDoubleSpinBox* m_minsb{}, *m_maxsb{};
        const AutomationModel* m_model {};
};
