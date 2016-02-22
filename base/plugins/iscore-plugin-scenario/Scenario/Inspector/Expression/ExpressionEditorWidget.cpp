#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QDebug>
#include <QtGlobal>
#include <QStringList>
#include <QPushButton>
#include <iscore/widgets/MarginLess.hpp>

#include "ExpressionEditorWidget.hpp"
#include <Scenario/Inspector/Expression/SimpleExpressionEditorWidget.hpp>

namespace Scenario
{
ExpressionEditorWidget::ExpressionEditorWidget(const iscore::DocumentContext& doc, QWidget *parent) :
    QWidget(parent),
    m_context{doc}
{
    m_mainLayout = new iscore::MarginLess<QVBoxLayout>{this};

    auto btnWidg = new QWidget{this};
    auto btnLay = new iscore::MarginLess<QHBoxLayout>{btnWidg};

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
            setExpression(*State::parseExpression(m_expression));
    });
}

State::Expression ExpressionEditorWidget::expression()
{
    State::Expression exp{};

    State::Expression* lastRel{};

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
            if(op == State::BinaryOperator::Or)
            {
                auto pOp = op;
                if(lastRel->parent()->is<State::BinaryOperator>())
                    pOp = lastRel->parent()->get<State::BinaryOperator>();

                // we're taking out the child of an "OR" node or of the root
                while (pOp != State::BinaryOperator::Or && lastRel->parent() != &exp )
                {
                    lastRel = lastRel->parent();
                    if(lastRel->is<State::BinaryOperator>())
                        pOp = lastRel->get<State::BinaryOperator>();
                }
            }
            if(op != State::BinaryOperator::None)
            {
                auto p = lastRel->parent();
                // remove link between parent and current
                auto oldC = p->back();
                auto last_it = (++p->children().rbegin()).base();
                p->removeChild(last_it);

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

void ExpressionEditorWidget::setExpression(State::Expression e)
{
    for(auto& elt : m_relations)
    {
        delete elt;
    }
    m_relations.clear();

    exploreExpression(e);
    if(!e.hasChildren())
        addNewRelation();

    m_expression = currentExpr();
}

void ExpressionEditorWidget::on_editFinished()
{
    auto ex = currentExpr();
    auto e = State::parseExpression(ex);
    if (m_expression == ex || (!e && !ex.isEmpty()))
        return;

    m_expression = ex;
    emit editingFinished();
}

void ExpressionEditorWidget::exploreExpression(State::Expression expr)
{
    const struct {
        public:
            const State::Expression& e;
            ExpressionEditorWidget& widg;
            using return_type = void;

            return_type operator()(const State::Relation& rel) const
            {
                widg.addNewRelation();
                widg.m_relations.back()->setRelation(rel);
            }

            return_type operator()(const State::Pulse&) const
            {
                ISCORE_TODO;
            }

            return_type operator()(const State::BinaryOperator op) const
            {
                auto a = e.childAt(0);
                auto b = e.childAt(1);

                widg.exploreExpression(a);
                widg.exploreExpression(b);

                for(int i = 1; i < widg.m_relations.size(); i++)
                {
                    if(widg.m_relations.at(i)->binOperator() == State::BinaryOperator::None )
                        widg.m_relations.at(i)->setOperator( op );
                }

                widg.m_relations.back()->setOperator( op );
            }

            return_type operator()(const State::UnaryOperator) const
            {
                ISCORE_TODO_("Implement unary operator");
                widg.exploreExpression(e.childAt(0));
            }

            return_type operator()(const InvisibleRootNodeTag) const
            {
                if(e.childCount() > 0)
                    widg.exploreExpression(e.childAt(0));
            }

    } visitor{expr, *this};

    return eggs::variants::apply(visitor, expr.impl());
}

QString ExpressionEditorWidget::currentExpr()
{
    auto exp = expression();
    return exp.toString();
}

void ExpressionEditorWidget::addNewRelation()
{
    auto relationEditor = new SimpleExpressionEditorWidget{m_context, m_relations.size(), this};
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
}
