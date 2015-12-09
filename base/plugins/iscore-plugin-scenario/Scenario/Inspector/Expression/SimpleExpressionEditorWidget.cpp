#include <State/Expression.hpp>
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

SimpleExpressionEditorWidget::SimpleExpressionEditorWidget(int index, QWidget* parent):
    QWidget(parent),
    id{index}
{
    auto mainLay = new QGridLayout{this};
    mainLay->setSpacing(1);

    m_address = new QLineEdit{this};
//    auto secondMember = new QWidget{this};

//    mainLay->addWidget(m_address);
//    mainLay->addWidget(secondMember);
/*
    auto hLay = new QHBoxLayout{mainLay};
    hLay->setContentsMargins(0,0,0,0);
    hLay->setSpacing(0);
*/
    m_ok = new QLabel{"/!\\ ", this};
    m_comparator = new QComboBox{this};
    m_value = new QLineEdit{this};
/*
    hLay->addWidget(m_ok);
    hLay->addWidget(m_comparator);
    hLay->addWidget(m_value);
*/
    auto addBtn = new QToolButton{this};
    addBtn->setText("+");
    auto rmBtn = new QToolButton{this};
    rmBtn->setText("-");
//    hLay->addWidget(addBtn);
//    hLay->addWidget(rmBtn);

    m_binOperator = new QComboBox{this};
//    mainLay->addWidget(m_binOperator);
    m_binOperator->addItem("");
    m_binOperator->addItem("and");
    m_binOperator->addItem("or");

    mainLay->addWidget(m_binOperator, 1, 0, 1, 1);
    mainLay->addWidget(m_address, 1, 1, 1, 1);
    mainLay->addWidget(m_comparator, 1, 3, 1, 1);
    mainLay->addWidget(m_value, 1, 4, 1, 3);
//    mainLay->addWidget(addBtn, 1, 1, 1, 1);
//    mainLay->addWidget(rmBtn, 1, 1, 1, 1);
/*    mainLay->setColumnStretch(2, 10);
    mainLay->setColumnStretch(3, 0);
    mainLay->setColumnStretch(4, 10);
*/
    mainLay->setColumnStretch(0, 100);
    m_binOperator->setMaximumWidth(5);
    mainLay->setColumnMinimumWidth(0,1);
    mainLay->setColumnStretch(0, 0);
    m_binOperator->setMaximumWidth(10);

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
            this, &SimpleExpressionEditorWidget::on_operatorChanged);

    m_ok->setVisible(false);
    m_value->setEnabled(false);

    m_comparator->addItem(""); //TODO if no value : is it a bang  ?
    m_comparator->addItem("==");
    m_comparator->addItem("!=");
    m_comparator->addItem(">");
    m_comparator->addItem("<");
    m_comparator->addItem(">=");
    m_comparator->addItem("<=");
}

iscore::Expression SimpleExpressionEditorWidget::expression()
{
    int i = 1;
    QString expr = currentRelation();
    m_validator.validate(expr, i);
    return *m_validator.get();
}

iscore::BinaryOperator SimpleExpressionEditorWidget::binOperator()
{
    switch (m_binOperator->currentIndex()) {
        case 1 : return iscore::BinaryOperator::And;
        case 2 : return iscore::BinaryOperator::Or;
        default : return iscore::BinaryOperator::None;
    }
}

void SimpleExpressionEditorWidget::setExpression(iscore::Expression e)
{
    auto stringExp = e.toString();
    if(e.childCount() == 1)
    {
        if(stringExp.indexOf("(") == 0)
            stringExp.remove(0,1);
        if(stringExp.endsWith(")"))
            stringExp.remove(stringExp.size()-1, 1);

        QStringList list = stringExp.split(" ");
        m_address->setText(list.first());

        if(list.size() > 1)
        {
            m_comparator->setCurrentText(list.at(1));
            m_value->setText(list.last());
        }
    }
    m_relation = stringExp;
    int i;
    m_ok->setVisible(m_validator.validate(m_relation, i) != QValidator::State::Acceptable);
}

void SimpleExpressionEditorWidget::setOperator(iscore::BinaryOperator o)
{
    switch (o) {
        case iscore::BinaryOperator::And :
            m_binOperator->setCurrentIndex(1); m_op = "and";
            break;
        case iscore::BinaryOperator::Or :
            m_binOperator->setCurrentIndex(2); m_op = "or";
            break;
        default :
            m_binOperator->setCurrentIndex(0); m_op = "";
            break;
        }
}

void SimpleExpressionEditorWidget::setOperator(iscore::UnaryOperator u)
{
    // TODO : add the unary operator
/*    switch (u) {
        case iscore::UnaryOperator::Not :
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

    m_ok->setVisible(m_validator.validate(expr, i) != QValidator::State::Acceptable );
    if(m_validator.validate(expr, i) == QValidator::State::Acceptable)
        emit editingFinished();
}

void SimpleExpressionEditorWidget::on_operatorChanged(int i)
{
    m_value->setEnabled(i != 0);
}


QString SimpleExpressionEditorWidget::currentRelation()
{
    QString expr =  m_address->text();
    if(m_comparator->currentIndex() > 0)
    {
        expr += " ";
        expr += m_comparator->currentText();
        expr += " ";
        expr += m_value->text();
    }
    return expr;
}

QString SimpleExpressionEditorWidget::currentOperator()
{
    return m_binOperator->currentText();
}

