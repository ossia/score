#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QDebug>
#include <QtGlobal>
#include <QStringList>
#include <QPushButton>

#include "ExpressionEditorWidget.hpp"
#include <Scenario/Inspector/Expression/SimpleExpressionEditorWidget.hpp>

ExpressionEditorWidget::ExpressionEditorWidget(QWidget *parent) :
    QWidget(parent)
{
    m_mainLayout = new QVBoxLayout{this};

    auto btnWidg = new QWidget{this};
    auto btnLay = new QHBoxLayout{btnWidg};

    auto validBtn = new QPushButton{"OK",btnWidg};
    auto cancelBtn = new QPushButton{tr("Cancel"),btnWidg};
    btnLay->addWidget(validBtn);
    btnLay->addWidget(cancelBtn);

    m_mainLayout->addWidget(btnWidg);

    connect(validBtn, &QPushButton::clicked,
            this, &ExpressionEditorWidget::on_editFinished);
    connect(cancelBtn, &QPushButton::clicked,
            this, [&] ()
    {
        if(m_expression.isEmpty()) {
            for(auto& elt : m_relations)
            {
                delete elt;
            }
            m_relations.clear();
            addNewRelation();
        }
        else
            setExpression(*iscore::parse(m_expression));
    });

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
            auto ANDmembers = m.split("and");
            for(auto n : ANDmembers)
            {
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
    auto relationEditor = new SimpleExpressionEditorWidget{m_relations.size(), this};
    m_relations.push_back(relationEditor);

    m_mainLayout->addWidget(relationEditor);
/*
 * TODO : this allow to remove the OK button but it crashes ...
    connect(relationEditor, &SimpleExpressionEditorWidget::editingFinished,
        this, &ExpressionEditorWidget::on_editFinished);
*/
    connect(relationEditor, &SimpleExpressionEditorWidget::addRelation,
            this, &ExpressionEditorWidget::addNewRelation);
    connect(relationEditor, &SimpleExpressionEditorWidget::removeRelation,
            this, &ExpressionEditorWidget::removeRelation);
}

void ExpressionEditorWidget::removeRelation(int index)
{
    for (int i = index; i < m_relations.size(); i++)
    {
        m_relations.at(i)->id--;
    }
    delete m_relations.at(index);
    m_relations.removeAt(index);
}

