#include <State/Expression.hpp>
#include <State/Relation.hpp>
#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QToolButton>

#include <Scenario/Inspector/ExpressionValidator.hpp>
#include "SimpleExpressionEditorWidget.hpp"
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SignalUtils.hpp>
#include <iscore/widgets/SetIcons.hpp>

namespace Scenario
{
SimpleExpressionEditorWidget::SimpleExpressionEditorWidget(
        const iscore::DocumentContext& doc, int index, QWidget* parent):
    QWidget(parent),
    id{index}
{
    auto mainLay = new iscore::MarginLess<QHBoxLayout>{this};

    m_binOperator = new QComboBox{this};

    auto plug = doc.findPlugin<Explorer::DeviceDocumentPlugin>();
    Explorer::DeviceExplorerModel* explorer{};
    if(plug)
        explorer = &plug->explorer;

    m_address = new Explorer::AddressAccessorEditWidget{explorer, this};
    m_ok = new QLabel{"/!\\ ", this};

    m_comparator = new QComboBox{this};


    m_value = new QLineEdit{this};

    auto btnWidg = new QWidget{this};
    auto btnLay = new iscore::MarginLess<QVBoxLayout>{btnWidg};
    auto rmBtn = new QToolButton{btnWidg};
    rmBtn->setText("-");
    rmBtn->setMaximumSize(30, 30);

    QIcon remIcon;
    makeIcons(&remIcon,
              QString(":/icons/condition_remove_on.png"),
              QString(":/icons/condition_remove_off.png"));

    rmBtn->setIcon(remIcon);

    btnLay->addWidget(rmBtn);

    // Main Layout

    mainLay->addWidget(m_ok);
    mainLay->addWidget(m_binOperator, 0, Qt::AlignHCenter);
    mainLay->addWidget(m_address, 10);
    mainLay->addWidget(m_comparator, 0, Qt::AlignHCenter);
    mainLay->addWidget(m_value, 2);

    mainLay->addWidget(btnWidg);

    // Connections

    connect(rmBtn, &QPushButton::clicked,
            this, [=] ()
    {
        emit removeTerm(id);
    });

    /// EDIT FINSHED
    connect(m_address, &Explorer::AddressAccessorEditWidget::addressChanged,
            this, [&] ()
    {
        on_editFinished();
    });
    connect(m_comparator, &QComboBox::currentTextChanged,
            this, [&] (QString) {on_editFinished();} );
    connect(m_value, &QLineEdit::editingFinished,
            this, &SimpleExpressionEditorWidget::on_editFinished);
    connect(m_binOperator, &QComboBox::currentTextChanged,
            this, [&] (QString) {on_editFinished(); });

    // enable value field
    connect(m_comparator,  SignalUtils::QComboBox_currentIndexChanged_int(),
            this, &SimpleExpressionEditorWidget::on_comparatorChanged);

    m_ok->setVisible(false);
    m_value->setEnabled(false);

    // Fill ComboBox

    m_binOperator->setObjectName("BinOpComboBox");
    m_comparator->setObjectName("BinOpComboBox");

    m_binOperator->addItem(" ");
    m_binOperator->addItem("&");
    m_binOperator->addItem("|");

    auto& lst = ExpressionEditorComparators();

    for(auto& c : lst)
    {
        m_comparator->addItem(c.second, QVariant::fromValue(c.first));
    }

    m_comparator->setCurrentText(lst.at(ExpressionEditorComparator::None));
}

State::Expression SimpleExpressionEditorWidget::relation()
{
    int i = 1;
    QString expr = currentRelation();

    m_validator.validate(expr, i);
    if(m_validator.validate(expr, i) == QValidator::State::Acceptable )
    {
        return *m_validator.get();
    }

    else
        return State::Expression{};
}

State::BinaryOperator SimpleExpressionEditorWidget::binOperator()
{
    switch (m_binOperator->currentIndex()) {
        case 1 : return State::BinaryOperator::And;
        case 2 : return State::BinaryOperator::Or;
        default : return State::BinaryOperator::None;
    }
}

void SimpleExpressionEditorWidget::setRelation(State::Relation r)
{
    auto lptr = r.lhs.target<State::Value>();
    auto rptr = r.rhs.target<State::Value>();
    if(lptr && rptr)
    {
        auto lv = *lptr;
        auto rv = *rptr;

        if(r.op == State::Relation::Operator::Equal && lv == rv)
        {
            m_comparator->setCurrentIndex(ExpressionEditorComparator::AlwaysTrue);
        }
        else
        {
            m_comparator->setCurrentIndex(ExpressionEditorComparator::AlwaysFalse);
        }
        m_address->setAddress(State::AddressAccessor{});
        m_value->setText("");
    }
    else
    {
        if(auto addr_ptr = r.lhs.target<State::Address>())
        {
            m_address->setAddress(*addr_ptr);
        }
        else if(auto acc_ptr = r.lhs.target<State::AddressAccessor>())
        {
            m_address->setAddress(*acc_ptr);
        }

        auto s = State::toString(r.rhs);
        m_value->setText(s);

        m_comparator->setCurrentIndex(static_cast<int>(r.op));

        m_relation = State::toString(r);

        int i;
        m_ok->setVisible(m_validator.validate(m_relation, i) != QValidator::State::Acceptable);
    }
}

void SimpleExpressionEditorWidget::setPulse(State::Pulse p)
{
    m_address->setAddress(p.address);
    m_value->setText("");

    m_comparator->setCurrentIndex(ExpressionEditorComparator::Pulse);
    m_relation = State::toString(p);

    int i;
    m_ok->setVisible(m_validator.validate(m_relation, i) != QValidator::State::Acceptable);

}

void SimpleExpressionEditorWidget::setOperator(State::BinaryOperator o)
{
    switch (o) {
        case State::BinaryOperator::And :
            m_binOperator->setCurrentIndex(1); m_op = "and";
            break;
        case State::BinaryOperator::Or :
            m_binOperator->setCurrentIndex(2); m_op = "or";
            break;
        default :
            m_binOperator->setCurrentIndex(0); m_op = "";
            break;
        }
}

void SimpleExpressionEditorWidget::setOperator(State::UnaryOperator u)
{
    // TODO : add the unary operator
/*    switch (u) {
        case State::UnaryOperator::Not :
            break;
        default:
            m_binOperator->setCurrentIndex(0);
            break;
    }
    */
}

void SimpleExpressionEditorWidget::on_editFinished()
{
    QString expr = currentRelation();
    if(expr == m_relation && m_op == currentOperator())
        return;

    int i = 1;
    m_op = currentOperator();
    m_relation = expr;
    bool b = m_validator.validate(expr, i) == QValidator::State::Acceptable;
    m_ok->setVisible(!b);
    if(b)
        emit editingFinished();
}

void SimpleExpressionEditorWidget::on_comparatorChanged(int i)
{
    switch(i)
    {
        case ExpressionEditorComparator::Equal:
        case ExpressionEditorComparator::Different:
        case ExpressionEditorComparator::Greater:
        case ExpressionEditorComparator::Lower:
        case ExpressionEditorComparator::GreaterEqual:
        case ExpressionEditorComparator::LowerEqual:
            m_address->setEnabled(true);
            m_value->setEnabled(true);
            break;

        case ExpressionEditorComparator::None:
        case ExpressionEditorComparator::Pulse:
            m_address->setEnabled(true);
            m_value->setEnabled(false);
            break;

        case ExpressionEditorComparator::AlwaysTrue:
        case ExpressionEditorComparator::AlwaysFalse:
            m_address->setEnabled(false);
            m_value->setEnabled(false);
            break;
    }
}


QString SimpleExpressionEditorWidget::currentRelation()
{
    QString addr = m_address->addressString();

    switch(m_comparator->currentIndex())
    {
    case ExpressionEditorComparator::Greater:
    case ExpressionEditorComparator::Lower:
    {
        QString expr = m_address->addressString();
        expr += " ";
        expr += m_comparator->currentText();
        expr += " ";
        expr += m_value->text();
        return expr;
    }
    case ExpressionEditorComparator::Equal:
        return addr + " == " + m_value->text();
    case ExpressionEditorComparator::GreaterEqual:
        return addr + " >= " + m_value->text();
    case ExpressionEditorComparator::LowerEqual:
        return addr + " <= " + m_value->text();
    case ExpressionEditorComparator::Different:
        return addr + " != " + m_value->text();

    case ExpressionEditorComparator::None:
        return "";

    case ExpressionEditorComparator::Pulse:
    {
        QString expr = m_address->addressString() + " impulse";
        return expr;
    }
    case ExpressionEditorComparator::AlwaysTrue:
    {
        return "true == true"; // TODO berk
    }
    case ExpressionEditorComparator::AlwaysFalse:
    {
        return "true == false"; // TODO berk
    }
    }

    return "";
}

QString SimpleExpressionEditorWidget::currentOperator()
{
    return m_binOperator->currentText();
}

const std::map<ExpressionEditorComparator, QString>&ExpressionEditorComparators()
{
    static const std::map<ExpressionEditorComparator, QString> map{
        { ExpressionEditorComparator::Equal, "=" },
        { ExpressionEditorComparator::Different, QString::fromUtf8("\u2260") },
        { ExpressionEditorComparator::Greater, ">" },
        { ExpressionEditorComparator::Lower, "<" },
        { ExpressionEditorComparator::GreaterEqual, QString::fromUtf8("\u2265") },
        { ExpressionEditorComparator::LowerEqual, QString::fromUtf8("\u2264") },
        { ExpressionEditorComparator::None, "" },
        { ExpressionEditorComparator::Pulse, "Pulse" },
        { ExpressionEditorComparator::AlwaysTrue, "True" },
        { ExpressionEditorComparator::AlwaysFalse, "False" }
    };

    return map;
}

}
