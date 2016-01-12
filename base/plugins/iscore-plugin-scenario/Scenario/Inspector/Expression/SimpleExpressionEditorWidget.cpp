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

namespace Scenario
{
SimpleExpressionEditorWidget::SimpleExpressionEditorWidget(int index, QWidget* parent):
    QWidget(parent),
    id{index}
{
    auto mainLay = new QHBoxLayout{this};
    mainLay->setSpacing(1);
    mainLay->setContentsMargins(0,0,0,0);

    // TODO : this f*ck'n Widget is here because QCombobox doesnt let itself resize correctly
    auto binWidg = new QWidget{this};
    auto binLay = new QHBoxLayout{binWidg};
    m_binOperator = new QComboBox{binWidg};
    binLay->setContentsMargins(0,0,0,0);
    binWidg->setMaximumWidth(30);

    binLay->addWidget(m_binOperator);

    m_address = new QLineEdit{this};
    m_ok = new QLabel{"/!\\ ", this};


    // Again ugly thing
    auto opWidg = new QWidget{this};
    auto opLay = new QHBoxLayout{opWidg};
    m_comparator = new QComboBox{opWidg};
    opLay->setContentsMargins(0,0,0,0);
    opWidg->setMaximumWidth(25);

    opLay->addWidget(m_comparator);

    m_value = new QLineEdit{this};

    auto btnWidg = new QWidget{this};
    auto btnLay = new QVBoxLayout{btnWidg};
    btnLay->setContentsMargins(0,0,0,0);
    btnLay->setSpacing(0);
    auto addBtn = new QToolButton{btnWidg};
    addBtn->setText("+");
    auto rmBtn = new QToolButton{btnWidg};
    rmBtn->setText("-");

    addBtn->setMaximumSize(20, 20);
    rmBtn->setMaximumSize(20, 20);

    btnLay->addWidget(rmBtn);
    btnLay->addWidget(addBtn);

    // Main Layout

    mainLay->addWidget(m_ok);
    mainLay->addWidget(binWidg, 1, Qt::AlignHCenter);
    mainLay->addWidget(m_address, 10);
    mainLay->addWidget(opWidg, 1, Qt::AlignHCenter);
    mainLay->addWidget(m_value, 5);

    mainLay->addWidget(btnWidg);

    mainLay->setContentsMargins(0,0,0,0);

    // Connections

    connect(addBtn, &QPushButton::clicked,
            this, &SimpleExpressionEditorWidget::addRelation);
    connect(rmBtn, &QPushButton::clicked,
            this, [=] ()
    {
        emit removeRelation(id);
    });

    /// EDIT FINSHED
    connect(m_address, &QLineEdit::editingFinished,
            this, &SimpleExpressionEditorWidget::on_editFinished);
    connect(m_comparator, &QComboBox::currentTextChanged,
            this, [&] (QString) {on_editFinished();} );
    connect(m_value, &QLineEdit::editingFinished,
            this, &SimpleExpressionEditorWidget::on_editFinished);
    connect(m_binOperator, &QComboBox::currentTextChanged,
            this, [&] (QString) {on_editFinished(); });

    // enable value field
    connect(m_comparator,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &SimpleExpressionEditorWidget::on_comparatorChanged);

    m_ok->setVisible(false);
    m_value->setEnabled(false);

    // Fill ComboBox

    m_binOperator->addItem(" ");
    m_binOperator->addItem("and");
    m_binOperator->addItem("or");

    m_comparatorList = State::opToString();

    for(auto c : m_comparatorList)
    {
        m_comparator->addItem(c);
    }

    m_comparator->setCurrentText(m_comparatorList[Comparator::None]);

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
    m_address->setText(r.relMemberToString(r.lhs));

    auto s = r.relMemberToString(r.rhs);
    /*
    bool isDouble;
    s.toDouble(&isDouble);
    if(!isDouble)
    {
        s.remove(0,1);
        s.remove(s.lastIndexOf("\""),1);
    }*/
    m_value->setText(s);

    m_comparator->setCurrentText(m_comparatorList[r.op]);

    m_relation = r.toString();

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
    m_value->setEnabled(m_comparator->currentText() != m_comparatorList[Comparator::None]);
}


QString SimpleExpressionEditorWidget::currentRelation()
{
    QString expr =  m_address->text();
    if(m_comparator->currentText() != m_comparatorList[Comparator::None])
    {
        expr += " ";
        expr += m_comparator->currentText();
        expr += " ";
        expr += m_value->text();

        /*
        bool ok;
        auto val = m_value->text();
        val.toDouble(&ok);
        if(!ok)
        {
            val.prepend("\"");
            val += "\"";
        }
        expr += " ";
        expr += m_comparator->currentText();
        expr += " ";
        expr += val;*/
    }
    return expr;
}

QString SimpleExpressionEditorWidget::currentOperator()
{
    return m_binOperator->currentText();
}
}
