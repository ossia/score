#include <QString>
#include <QValidator>

#include "AddressLineEdit.hpp"
#include <State/Widgets/AddressValidator.hpp>

class QWidget;

AddressLineEdit::AddressLineEdit(QWidget* parent):
    QLineEdit{parent}
{
    connect(this, &QLineEdit::textChanged,
            this, [&] (const QString& str) {
        QString s = str;
        int i = 0;
        if(m_validator.validate(s, i) == QValidator::State::Acceptable)
        {
            this->setStyleSheet("QLineEdit { background: black; }");
        }
        else
        {
            this->setStyleSheet("QLineEdit { background: #660000; }");
        }
    });
}
