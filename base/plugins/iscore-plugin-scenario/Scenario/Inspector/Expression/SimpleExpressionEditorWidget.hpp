#pragma once

#include <QWidget>
#include <Scenario/Inspector/ExpressionValidator.hpp>

class QLineEdit;
class QComboBox;
class QLabel;

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
        void setOperator(int);

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
        QLineEdit * m_value{};

        QComboBox* m_comparator{};
        QComboBox* m_operator{};

        ExpressionValidator<iscore::Expression> m_validator;
};

