#pragma once
#include <QLineEdit>
#include "AddressValidator.hpp"

class AddressLineEdit : public QLineEdit
{
    public:
        AddressLineEdit(QWidget* parent);

    private:
        AddressValidator m_validator;
};
