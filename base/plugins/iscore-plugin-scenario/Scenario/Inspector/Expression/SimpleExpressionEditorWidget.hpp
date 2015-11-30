#pragma once

#include <Scenario/Inspector/ExpressionValidator.hpp>

#include <QString>
#include <QWidget>

#include <State/Expression.hpp>

class QComboBox;
class QLabel;
class QLineEdit;

class SimpleExpressionEditorWidget : public QWidget
{
	Q_OBJECT
    public:
        SimpleExpressionEditorWidget(QWidget* parent = 0);

        iscore::Expression expression();
        iscore::BinaryOperator binOperator();

    public slots:
        void setExpression(iscore::Expression e);
        void setOperator(iscore::BinaryOperator o);
        void setOperator(iscore::UnaryOperator u);

        QString currentRelation();
        QString currentOperator();
    signals:
        void editingFinished();
        void addRelation();
        void removeRelation();

    private slots:
        void on_editFinished();
        void on_operatorChanged(int i);
// TODO on_modelChanged()

    private:

        QLabel* m_ok{};

        QLineEdit* m_address{};
        QComboBox* m_comparator{};
        QLineEdit * m_value{};
        QComboBox* m_binOperator{};

        ExpressionValidator<iscore::Expression> m_validator;
        QString m_relation{};
        QString m_op{};
};

