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
        SimpleExpressionEditorWidget(int index, QWidget* parent = 0);

        iscore::Expression relation();
        iscore::BinaryOperator binOperator();

        int id;

    public slots:
        void setRelation(iscore::Relation r);
        void setOperator(iscore::BinaryOperator o);
        void setOperator(iscore::UnaryOperator u);

        QString currentRelation();
        QString currentOperator();
    signals:
        void editingFinished();
        void addRelation();
        void removeRelation(int index);

    private slots:
        void on_editFinished();
        void on_operatorChanged(int i);
// TODO on_modelChanged() -> done in parent inspector (i.e. event), no ?

    private:

        QLabel* m_ok{};

        QLineEdit* m_address{};
        QComboBox* m_comparator{};
        QLineEdit * m_value{};
        QComboBox* m_binOperator{};

        ExpressionValidator<iscore::Expression> m_validator;
        QString m_relation{};
        QString m_op{};

        QMap<iscore::Relation::Operator, QString> m_comparatorList;
};

