#include "AddressLineEdit.hpp"
AddressLineEdit::AddressLineEdit(QWidget* parent):
    QLineEdit{parent}
{
    connect(this, &QLineEdit::textChanged,
            this, [&] (const QString& str) {
        QString s = str;
        int i = 0;
        if(m_validator.validate(s, i))
        {
            this->setStyleSheet("QLineEdit { background: white; }");
        }
        else
        {
            this->setStyleSheet("QLineEdit { background: #f6dbd9; }");
        }
    });
}
