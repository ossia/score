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
    auto mainLay = new QHBoxLayout{this};
    mainLay->setSpacing(1);
    mainLay->setContentsMargins(0,0,0,0);

    // TODO : this f*ck Widget is here because QCombobox doesnt let itself resize correctly
    auto binWidg = new QWidget{this};
    auto binLay = new QHBoxLayout{binWidg};
    m_binOperator = new QComboBox{binWidg};
    binLay->addWidget(m_binOperator);
    binLay->setContentsMargins(0,0,0,0);
    binWidg->setMaximumWidth(30);

    m_address = new QLineEdit{this};
    m_ok = new QLabel{"/!\\ ", this};

    auto opWidg = new QWidget{this};
    auto opLay = new QHBoxLayout{opWidg};
    m_comparator = new QComboBox{opWidg};
    opLay->addWidget(m_comparator);
    opLay->setContentsMargins(0,0,0,0);
    opWidg->setMaximumWidth(25);

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

    m_binOperator->addItem(" ");
    m_binOperator->addItem("and");
    m_binOperator->addItem("or");

    mainLay->addWidget(m_ok);
    mainLay->addWidget(binWidg, 1, Qt::AlignHCenter);
    mainLay->addWidget(m_address, 10);
    mainLay->addWidget(opWidg, 1, Qt::AlignHCenter);
    mainLay->addWidget(m_value, 5);

    mainLay->addWidget(btnWidg);

    mainLay->setContentsMargins(0,0,0,0);


//    adjustWidth();

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

    m_comparator->addItem("");
    m_comparator->addItem("==");
    m_comparator->addItem("!=");
    m_comparator->addItem(">");
    m_comparator->addItem("<");
    m_comparator->addItem(">=");
    m_comparator->addItem("<=");

//    adjustWidth();
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

void SimpleExpressionEditorWidget::adjustWidth()
{
    m_binOperator->setFixedWidth(40);
    m_comparator->setFixedWidth(35);
}

void SimpleExpressionEditorWidget::on_editFinished()
{
    qDebug() << m_binOperator->minimumWidth() << " < " << m_binOperator->width() << " < " << m_binOperator->maximumWidth() << " hint " << m_binOperator->sizeHint().width();
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

