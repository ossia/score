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

    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    auto btnWidg = new QWidget{this};
    auto btnLay = new QHBoxLayout{btnWidg};
    btnLay->setSpacing(0);
    btnLay->setContentsMargins(0, 0, 0, 0);

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

    exploreExpression(e);
}

void ExpressionEditorWidget::on_editFinished()
{
    if (m_expression == currentExpr())
        return;

    m_expression = currentExpr();
    emit editingFinished();
}

void ExpressionEditorWidget::exploreExpression(iscore::Expression e)
{
    int c = e.childCount();
    switch (c)
    {
    case 2: {
        auto a = e.childAt(0);
        auto b = e.childAt(1);
        if(a.hasChildren())
            exploreExpression(a);
        else
        {
            addNewRelation();
            m_relations.back()->setRelation( a.get<iscore::Relation>() );
        }

        if(b.hasChildren())
        {
            if(e.is<iscore::BinaryOperator>())
            {
                m_relations.back()->setOperator( e.get<iscore::BinaryOperator>() );
            }
            exploreExpression(b);
        }
        else
        {
            if(e.is<iscore::BinaryOperator>())
            {
                addNewRelation();
                m_relations.back()->setOperator( e.get<iscore::BinaryOperator>() );
                m_relations.back()->setRelation( b.get<iscore::Relation>() );
            }
        }
    }
        break;
    case 1:
        qDebug() << "no unary op for now"; exploreExpression(e.childAt(0)); break;
    case 0:
        addNewRelation();
        if(e.is<iscore::Relation>())
            m_relations.back()->setRelation(e.get<iscore::Relation>());
        break;
    default:
        qDebug() << "unexpected child count"; break;
    }
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

