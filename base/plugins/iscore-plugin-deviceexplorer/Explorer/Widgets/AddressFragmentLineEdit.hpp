#pragma once
#include <QLineEdit>
#include <QValidator>
#include <State/Address.hpp>


class AddressFragmentValidator : public QValidator
{
    public:
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
        AddressFragmentLineEdit()
        {
            setValidator(new AddressFragmentValidator);
        }
};
