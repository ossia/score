#include "AddressLineEdit.hpp"

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
