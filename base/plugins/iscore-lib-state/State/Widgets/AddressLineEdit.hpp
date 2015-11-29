#pragma once
#include <qlineedit.h>

#include "AddressValidator.hpp"

class QWidget;

/**
 * @brief The AddressLineEdit class
 *
 * Used to input an address. Changes colors to red-ish if it is invalid.
 */
class AddressLineEdit : public QLineEdit
{
    public:
        explicit AddressLineEdit(QWidget* parent);

    private:
        AddressValidator m_validator;
};
