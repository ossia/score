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

    public slots:
        void setExpression(iscore::Expression e);

    signals:
        void editingFinished();

    private slots:
        void on_editFinished();
        void on_operatorChanged(int i);
// TODO on_modelChanged()

        QString currentExpr();
    private:

        QLabel* m_ok{};
        QLineEdit* m_address{};
        QLineEdit * m_value{};

        QComboBox* m_operator{};

        ExpressionValidator<iscore::Expression> m_validator;
};

