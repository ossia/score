#pragma once
#include <QLineEdit>
#include <QValidator>
#include <State/Address.hpp>


class AddressFragmentValidator : public QValidator
{
    public:
        State validate(QString& s, int& pos) const override
        {
            return iscore::Address::validateFragment(s)
                    ? State::Acceptable
                    : State::Invalid;
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
