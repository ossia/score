#include "ExpressionEditorWidget.hpp"

#include "Scenario/Inspector/Expression/SimpleExpressionEditorWidget.hpp"

#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>

ExpressionEditorWidget::ExpressionEditorWidget(QWidget *parent) :
    QWidget(parent)
{
    m_mainLayout = new QVBoxLayout{this};
    addNewRelation();
}

iscore::Expression ExpressionEditorWidget::expression()
{
    auto e = *iscore::parse(currentExpr());
    return e;
}

void ExpressionEditorWidget::setExpression(iscore::Expression e)
{
    for(auto& elt : m_relations)
    {
        delete elt;
    }
    m_relations.clear();
    auto es = e.toString();
    m_expression = es;
    if(!es.isEmpty())
    {
        auto ORmembers = es.split("or");
        for(auto m : ORmembers)
        {
            qDebug() << m;
            auto ANDmembers = m.split("and");
            for(auto n : ANDmembers)
            {
                qDebug() << "n " << n;
                if(n.isEmpty())
                    break;
                addNewRelation();
                m_relations.back()->setExpression(*iscore::parse(n));
                m_relations.back()->setOperator(iscore::BinaryOperator::And);
            }
            m_relations.back()->setOperator(iscore::BinaryOperator::Or);
        }
        m_relations.back()->setOperator(iscore::BinaryOperator::None);
    }
    if(m_relations.size() == 0)
        addNewRelation(); //if no expression in model, add a void one
}

void ExpressionEditorWidget::on_editFinished()
{
    if (m_expression == currentExpr())
        return;

    m_expression = currentExpr();
    emit editingFinished();
}

QString ExpressionEditorWidget::currentExpr()
{
    QString expr;
    for(auto r : m_relations)
    {
        expr += r->currentRelation();
        expr += " ";
        expr += r->currentOperator();
        expr += " ";
    }
    while(expr.endsWith(" "))
    {
        expr.remove(expr.size()-1, 1);
    }
    return expr;
}

void ExpressionEditorWidget::addNewRelation()
{
    auto relationEditor = new SimpleExpressionEditorWidget{this};
    m_relations.push_back(relationEditor);

    m_mainLayout->addWidget(relationEditor);

    connect(relationEditor, &SimpleExpressionEditorWidget::editingFinished,
        this, &ExpressionEditorWidget::on_editFinished);
    connect(relationEditor, &SimpleExpressionEditorWidget::addRelation,
            this, &ExpressionEditorWidget::addNewRelation);

    //TODO remove relation
}

