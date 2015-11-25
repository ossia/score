#include "SimpleExpressionEditorWidget.hpp"

#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <State/Expression.hpp>

SimpleExpressionEditorWidget::SimpleExpressionEditorWidget(QWidget* parent):
    QWidget(parent)
{
    auto mainLay = new QVBoxLayout{this};
    mainLay->setSpacing(1);

    m_address = new QLineEdit{this};
    auto secondMember = new QWidget{this};

    mainLay->addWidget(m_address);
    mainLay->addWidget(secondMember);

    auto hLay = new QHBoxLayout{secondMember};
    hLay->setContentsMargins(0,0,0,0);
    hLay->setSpacing(1);

    m_ok = new QLabel{"/!\\ ", secondMember};
    m_operator = new QComboBox{secondMember};
    m_value = new QLineEdit{secondMember};

    hLay->addWidget(m_ok);
    hLay->addWidget(m_operator);
    hLay->addWidget(m_value);
    hLay->addWidget(new QWidget{secondMember});

    connect(m_address, &QLineEdit::editingFinished,
            this, &SimpleExpressionEditorWidget::on_editFinished);
    connect(m_value, &QLineEdit::editingFinished,
            this, &SimpleExpressionEditorWidget::on_editFinished);
    connect(m_operator,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &SimpleExpressionEditorWidget::on_operatorChanged);

    m_ok->setVisible(false);
    m_value->setEnabled(false);

    m_operator->addItem(" "); //TODO if no value : is it a bang or a boolean comp ?
    m_operator->addItem("==");
    m_operator->addItem("!=");
    m_operator->addItem(">");
    m_operator->addItem("<");
    m_operator->addItem(">=");
    m_operator->addItem("<=");

}

iscore::Expression SimpleExpressionEditorWidget::expression()
{
    int i = 1;
    QString expr = currentExpr();
    m_validator.validate(expr, i);
    return *m_validator.get();
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
            m_operator->setCurrentText(list.at(1));
            m_value->setText(list.last());
        }
    }
}

void SimpleExpressionEditorWidget::on_editFinished()
{

    int i = 1;
    QString expr = currentExpr();
    m_ok->setVisible(m_validator.validate(expr, i) != QValidator::State::Acceptable );
    if(m_validator.validate(expr, i) == QValidator::State::Acceptable)
        emit editingFinished();
}

void SimpleExpressionEditorWidget::on_operatorChanged(int i)
{
    m_value->setEnabled(i != 0);
}

QString SimpleExpressionEditorWidget::currentExpr()
{
    QString expr = m_address->text();
    if(m_operator->currentIndex() > 0)
    {
        expr += " ";
        expr += m_operator->currentText();
        expr += " ";
        expr += m_value->text();
    }
    return expr;
}

