#include "ExpressionEditorWidget.hpp"

#include "Scenario/Inspector/Expression/SimpleExpressionEditorWidget.hpp"

#include <QLabel>
#include <QComboBox>

ExpressionEditorWidget::ExpressionEditorWidget(QWidget *parent) :
    QWidget(parent)
{
    addNewRelation();
}

iscore::Expression ExpressionEditorWidget::expression()
{
    return m_relations.first()->expression();
}

void ExpressionEditorWidget::setExpression(iscore::Expression e)
{
    m_relations.first()->setExpression(e);
}

void ExpressionEditorWidget::on_editFinished()
{

}

QString ExpressionEditorWidget::currentExpr()
{
    return m_relations.first()->currentExpr();
}

void ExpressionEditorWidget::addNewRelation()
{
    auto relationEditor = new SimpleExpressionEditorWidget{this};
    m_relations.push_back(relationEditor);

    connect(relationEditor, &SimpleExpressionEditorWidget::editingFinished,
        this, &ExpressionEditorWidget::editingFinished);
    connect(relationEditor, &SimpleExpressionEditorWidget::composeExpression,
            this, &ExpressionEditorWidget::addNewRelation);
    ISCORE_TODO;
}

