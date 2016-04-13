#pragma once
#include <QLineEdit>
#include <QValidator>
#include <State/Address.hpp>

// TODO MOVEME libstate
class AddressFragmentValidator : public QValidator
{
    public:
        using QValidator::QValidator;
        QValidator::State validate(QString& s, int& pos) const override
        {
            return ::State::Address::validateFragment(s)
                    ? QValidator::State::Acceptable
                    : QValidator::State::Invalid;
        }
};

class AddressFragmentLineEdit : public QLineEdit
{
    public:
        AddressFragmentLineEdit(QWidget* parent):
            QLineEdit{parent}
        {
            setValidator(new AddressFragmentValidator{this});
        }
};
