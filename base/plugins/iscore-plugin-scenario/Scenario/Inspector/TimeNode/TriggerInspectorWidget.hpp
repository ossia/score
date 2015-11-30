#pragma once

#include <QWidget>

#include <State/Expression.hpp>

class InspectorWidgetBase;
class QPushButton;
class ExpressionEditorWidget;
class TimeNodeModel;
class TriggerCommandFactoryList;

class TriggerInspectorWidget final : public QWidget
{
    public:
        TriggerInspectorWidget(
                const TriggerCommandFactoryList& fact,
                const TimeNodeModel& object,
                InspectorWidgetBase* parent);

        void on_triggerChanged();

        void createTrigger();
        void removeTrigger();

        void on_triggerActiveChanged();
        void HideRmButton();

        void updateExpression(const iscore::Trigger&);

    private:
        const TriggerCommandFactoryList& m_triggerCommandFactory;
        const TimeNodeModel& m_model;

        InspectorWidgetBase* m_parent;

        QPushButton* m_addTrigBtn{};
        QPushButton* m_rmTrigBtn{};

        ExpressionEditorWidget* m_exprEditor{};
};
