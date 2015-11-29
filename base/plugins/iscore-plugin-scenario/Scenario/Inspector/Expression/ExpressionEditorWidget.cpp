#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QDebug>
#include <QtGlobal>
#include <QStringList>

#include "ExpressionEditorWidget.hpp"
#include <Scenario/Inspector/Expression/SimpleExpressionEditorWidget.hpp>

ExpressionEditorWidget::ExpressionEditorWidget(QWidget *parent) :
    QWidget(parent)
{
    m_mainLayout = new QVBoxLayout{this};
    addNewRelation();
}

iscore::Expression ExpressionEditorWidget::expression()
{
    auto e = *iscore::parse(currentExpr());
    qDebug() << currentExpr();
    qDebug() << e.toString();
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
    if(!es.isEmpty())
    {
        auto ORmembers = es.split("or");
        for(auto m : ORmembers)
        {
            auto ANDmembers = m.split("and");
            for(auto n : ANDmembers)
            {
                qDebug() << n;
                if(n.isEmpty())
                    break;
                addNewRelation();
                m_relations.back()->setExpression(*iscore::parse(n));
                m_relations.back()->setOperator(iscore::BinaryOperator::And);
            }
            m_relations.back()->setOperator(iscore::BinaryOperator::Or);
        }
        m_relations.back()->setOperator(0);
    }
    if(m_relations.size() == 0)
        addNewRelation(); //if no expression in model, add a void one
}

void ExpressionEditorWidget::on_editFinished()
{

}

QString ExpressionEditorWidget::currentExpr() //TODO : why not use Expression instead ?
{
    QString expr;
    for(auto r : m_relations)
    {
        expr += r->currentRelation();
        expr += " ";
        expr += r->currentOperator();
        expr += " ";
    }
    return expr;
}

void ExpressionEditorWidget::addNewRelation()
{
    auto relationEditor = new SimpleExpressionEditorWidget{this};
    m_relations.push_back(relationEditor);

    m_mainLayout->addWidget(relationEditor);

    connect(relationEditor, &SimpleExpressionEditorWidget::editingFinished,
        this, &ExpressionEditorWidget::editingFinished);
    connect(relationEditor, &SimpleExpressionEditorWidget::addRelation,
            this, &ExpressionEditorWidget::addNewRelation);

    //TODO remove relation
}

