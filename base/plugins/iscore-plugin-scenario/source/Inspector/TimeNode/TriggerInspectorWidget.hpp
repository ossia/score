#pragma once

#include <QWidget>
#include "Inspector/ExpressionValidator.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include "Document/TimeNode/TimeNodeModel.hpp"

class QLineEdit;
class QPushButton;

class TriggerInspectorWidget : public QWidget
{
    public:
        TriggerInspectorWidget(const TimeNodeModel& object, InspectorWidgetBase* parent);

    public slots:
        void on_triggerChanged();

        void createTrigger();
        void removeTrigger();

        void on_triggerActiveChanged();

        void updateExpression(QString);

    private:
        const TimeNodeModel& m_model;

        InspectorWidgetBase* m_parent;

        QLineEdit* m_triggerLineEdit{};
        QPushButton* m_addTrigBtn{};
        QPushButton* m_rmTrigBtn{};
        ExpressionValidator<iscore::Trigger> m_validator;
};
