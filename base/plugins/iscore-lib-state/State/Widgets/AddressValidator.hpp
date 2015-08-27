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
class AddressValidator : public QValidator
{
    public:
        State validate(QString& s, int& pos) const
        {
            // TODO use the middle state for when the
            // device / address is valid but not in the tree.
            return iscore::Address::validateString(s)
                    ? State::Acceptable
                    : State::Invalid;
        }
};
