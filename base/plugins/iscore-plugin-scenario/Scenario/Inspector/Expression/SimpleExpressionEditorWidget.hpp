#pragma once

#include <Scenario/Inspector/ExpressionValidator.hpp>

#include <QString>
#include <QWidget>

#include <State/Expression.hpp>

class QComboBox;
class QLabel;
class QLineEdit;
namespace DeviceExplorer {
class AddressEditWidget; }

namespace Scenario
{
// TODO move in plugin state
class SimpleExpressionEditorWidget final : public QWidget
{
    Q_OBJECT
    public:
        SimpleExpressionEditorWidget(const iscore::DocumentContext&, int index, QWidget* parent = 0);

        State::Expression relation();
        State::BinaryOperator binOperator();

        int id;

        void setRelation(State::Relation r);
        void setOperator(State::BinaryOperator o);
        void setOperator(State::UnaryOperator u);

        QString currentRelation();
        QString currentOperator();

    signals:
        void editingFinished();
        void addRelation();
        void removeRelation(int index);

    private:
        void on_editFinished();
        void on_comparatorChanged(int i);
// TODO on_modelChanged() -> done in parent inspector (i.e. event), no ?

        QLabel* m_ok{};

        DeviceExplorer::AddressEditWidget* m_address{};
        QComboBox* m_comparator{};
        QLineEdit * m_value{};
        QComboBox* m_binOperator{};

        ExpressionValidator<State::Expression> m_validator;
        QString m_relation{};
        QString m_op{};

        QMap<State::Relation::Operator, QString> m_comparatorList;
};
}
