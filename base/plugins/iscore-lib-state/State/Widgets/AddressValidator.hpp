#pragma once
#include <State/Address.hpp>
#include <QValidator>

/**
 * @brief The AddressValidator class
 *
 * A QValidator that checks if an address has a correct syntax.
 *
 * For instance :
 *  - "device:/dada/dodo" would work.
 *  - "/;/.f,v ç'tế&'" would not.
 */
class AddressValidator final : public QValidator
{
    public:
        State validate(QString& s, int& pos) const override
        {
            return iscore::Address::validateString(s)
                    ? State::Acceptable
                    : State::Intermediate;
        }
};
