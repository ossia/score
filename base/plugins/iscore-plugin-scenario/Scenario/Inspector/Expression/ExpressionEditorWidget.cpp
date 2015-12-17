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
            setExpression(*iscore::parseExpression(m_expression));
    });
}

iscore::Expression ExpressionEditorWidget::expression()
{
    iscore::Expression exp{};

    iscore::Expression* lastRel{};

    // will keep containing the last relation added

    for(auto r : m_relations)
    {
        if(!exp.hasChildren())
        {
            // add the first node : simple relation
            exp.emplace_back(r->relation(), &exp);
            lastRel = &(exp.back());
        }
        else
        {
            auto op = r->binOperator();
            if(op == iscore::BinaryOperator::Or)
            {
                auto pOp = op;
                if(lastRel->parent()->is<iscore::BinaryOperator>())
                    pOp = lastRel->parent()->get<iscore::BinaryOperator>();

                // we're taking out the child of an "OR" node or of the root
                while (pOp != iscore::BinaryOperator::Or && lastRel->parent() != &exp )
                {
                    lastRel = lastRel->parent();
                    if(lastRel->is<iscore::BinaryOperator>())
                        pOp = lastRel->get<iscore::BinaryOperator>();
                }
            }
            if(op != iscore::BinaryOperator::None)
            {
                auto p = lastRel->parent();
                // remove link between parent and current
                auto oldC = p->back();
                p->removeChild(p->end()--);

                // insert operator
                p->emplace_back(op, p);
                auto& nOp = p->front();

                // recreate link
                oldC.setParent(&nOp);
                nOp.push_back(oldC);

                // add the relation as child of the inserted operator
                nOp.emplace_back(r->relation(), &nOp);
            }
        }
    }
//    qDebug() << "-----------" << exp.toString() << "-----------";
    return exp;
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
    auto ex = currentExpr();
    auto e = iscore::parseExpression(m_expression);
    if (m_expression == ex && !e)
        return;

    m_expression = ex;
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
            exploreExpression(b);
            if(e.is<iscore::BinaryOperator>())
            {
                for(int i = 1; i<m_relations.size(); i++)
                {
                    if(m_relations.at(i)->binOperator() == iscore::BinaryOperator::None )
                        m_relations.at(i)->setOperator( e.get<iscore::BinaryOperator>() );
                }
            }
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
    auto exp = expression();
    return exp.toString();
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

