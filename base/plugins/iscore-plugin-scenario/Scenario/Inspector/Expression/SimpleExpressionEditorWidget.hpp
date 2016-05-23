#pragma once

#include <Scenario/Inspector/ExpressionValidator.hpp>

#include <QString>
#include <QWidget>

#include <State/Expression.hpp>

class QComboBox;
class QLabel;
class QLineEdit;
namespace Explorer {
class AddressAccessorEditWidget;
}


namespace Scenario
{

enum ExpressionEditorComparator
{
    // Same as in Relation
    Equal,
    Different,
    Greater,
    Lower,
    GreaterEqual,
    LowerEqual,
    None,

    // Additional ones
    Pulse,
    AlwaysTrue,
    AlwaysFalse
};

inline const std::map<ExpressionEditorComparator, QString>& ExpressionEditorComparators();

// TODO move in plugin state
class SimpleExpressionEditorWidget final : public QWidget
{
    Q_OBJECT
    public:
        SimpleExpressionEditorWidget(const iscore::DocumentContext&, int index, QWidget* parent = nullptr);

        State::Expression relation();
        State::BinaryOperator binOperator();

        int id;

        void setRelation(State::Relation r);
        void setPulse(State::Pulse r);
        void setOperator(State::BinaryOperator o);
        void setOperator(State::UnaryOperator u);

        QString currentRelation();
        QString currentOperator();

    signals:
        void editingFinished();
        void addTerm();
        void removeTerm(int index);

    private:
        void on_editFinished();
        void on_comparatorChanged(int i);
// TODO on_modelChanged() -> done in parent inspector (i.e. event), no ?

        QLabel* m_ok{};

        Explorer::AddressAccessorEditWidget* m_address{};
        QComboBox* m_comparator{};
        QLineEdit * m_value{};
        QComboBox* m_binOperator{};

        ExpressionValidator<State::Expression> m_validator;
        QString m_relation{};
        QString m_op{};

        QWidget* m_wrapper{};
};
}

Q_DECLARE_METATYPE(Scenario::ExpressionEditorComparator)
