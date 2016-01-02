#pragma once
#include <Scenario/Inspector/ExpressionValidator.hpp>

#include <QString>
#include <QVector>
#include <QWidget>

#include <State/Expression.hpp>

/* ****************************************************************
 * This class contain an aggregation of simples expressions.
 *
 * We assume that the global expression is "linear"
 * (without parenthesis)
 *
 * ****************************************************************/

class QVBoxLayout;
class SimpleExpressionEditorWidget;

class ExpressionEditorWidget : public QWidget
{
        Q_OBJECT
    public:
        explicit ExpressionEditorWidget(QWidget *parent = 0);

        State::Expression expression();
        void setExpression(State::Expression e);

    signals:
        void editingFinished();


    private:
        void on_editFinished();
//	void on_operatorChanged(int i);
// TODO on_modelChanged()

        void exploreExpression(State::Expression e);

        QString currentExpr();
        void addNewRelation();
        void removeRelation(int index);
        QVector<SimpleExpressionEditorWidget*> m_relations;

        QVBoxLayout* m_mainLayout{};

        ExpressionValidator<State::Expression> m_validator;

        QString m_expression{};
};

