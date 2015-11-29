#include <State/Expression.hpp>
#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QToolButton>

#include "Scenario/Inspector/ExpressionValidator.hpp"
#include "SimpleExpressionEditorWidget.hpp"

SimpleExpressionEditorWidget::SimpleExpressionEditorWidget(QWidget* parent):
    QWidget(parent)
{
    auto mainLay = new QVBoxLayout{this};
    mainLay->setSpacing(1);

    m_address = new QLineEdit{this};
    auto secondMember = new QWidget{this};
    m_operator = new QComboBox{this};

    mainLay->addWidget(m_address);
    mainLay->addWidget(secondMember);
    mainLay->addWidget(m_operator);

    m_operator->addItem("");
    m_operator->addItem("and");
    m_operator->addItem("or");

    auto hLay = new QHBoxLayout{secondMember};
    hLay->setContentsMargins(0,0,0,0);
    hLay->setSpacing(1);

    m_ok = new QLabel{"/!\\ ", secondMember};
    m_comparator = new QComboBox{secondMember};
    m_value = new QLineEdit{secondMember};

    connect(m_comparator, &QComboBox::currentTextChanged,
            this, [&] (QString) {on_editFinished();} );
    hLay->addWidget(m_ok);
    hLay->addWidget(m_comparator);
    hLay->addWidget(m_value);

    auto addBtn = new QToolButton{secondMember};
    addBtn->setText("+");
    auto rmBtn = new QToolButton{secondMember};
    rmBtn->setText("-");
    hLay->addWidget(addBtn);
    hLay->addWidget(rmBtn);

    connect(addBtn, &QPushButton::clicked,
            this, &SimpleExpressionEditorWidget::addRelation);
    connect(rmBtn, &QPushButton::clicked,
            this, &SimpleExpressionEditorWidget::removeRelation);

    connect(m_address, &QLineEdit::editingFinished,
            this, &SimpleExpressionEditorWidget::on_editFinished);
    connect(m_value, &QLineEdit::editingFinished,
            this, &SimpleExpressionEditorWidget::on_editFinished);
    connect(m_comparator,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &SimpleExpressionEditorWidget::on_operatorChanged);

    m_ok->setVisible(false);
    m_value->setEnabled(false);

    m_comparator->addItem(" "); //TODO if no value : is it a bang  ?
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
    switch (m_operator->currentIndex()) {
        case 0 : return iscore::BinaryOperator::And;
        case 1 : return iscore::BinaryOperator::Or;
        default : return iscore::BinaryOperator::And;
    }
}

void SimpleExpressionEditorWidget::setExpression(iscore::Expression e)
{
    if(e.childCount() == 1)
    {
        auto stringExp = e.toString();

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
}

void SimpleExpressionEditorWidget::setOperator(iscore::BinaryOperator o)
{
    switch (o) {
        case iscore::BinaryOperator::And :
            m_operator->setCurrentIndex(1);
            break;
        case iscore::BinaryOperator::Or :
            m_operator->setCurrentIndex(2);
            break;
        default:
            break;
        }
}

void SimpleExpressionEditorWidget::setOperator(int)
{
    m_operator->setCurrentIndex(0);
}

void SimpleExpressionEditorWidget::on_editFinished()
{
/*    int i = 1;
    QString expr = currentRelation();
    m_ok->setVisible(m_validator.validate(expr, i) != QValidator::State::Acceptable );
    if(m_validator.validate(expr, i) == QValidator::State::Acceptable)
        emit editingFinished();  */
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
    QString op = m_operator->isHidden() ? "" : m_operator->currentText();
    return op;
}

