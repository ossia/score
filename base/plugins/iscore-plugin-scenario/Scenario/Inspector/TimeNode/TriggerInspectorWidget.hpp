#pragma once

#include <QWidget>
#include <Scenario/Inspector/ExpressionValidator.hpp>
#include <Scenario/Inspector/Expression/SimpleExpressionEditorWidget.hpp>

#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

class QLineEdit;
class QPushButton;

class TriggerInspectorWidget final : public QWidget
{
    public:
        TriggerInspectorWidget(
                const TimeNodeModel& object,
                InspectorWidgetBase* parent);

    public slots:
        void on_triggerChanged();

        void createTrigger();
        void removeTrigger();

        void on_triggerActiveChanged();
        void HideRmButton();

        void updateExpression(iscore::Trigger);

    private:
        const TimeNodeModel& m_model;

        InspectorWidgetBase* m_parent;

        QPushButton* m_addTrigBtn{};
        QPushButton* m_rmTrigBtn{};

        SimpleExpressionEditorWidget* m_exprEditor{};
};
