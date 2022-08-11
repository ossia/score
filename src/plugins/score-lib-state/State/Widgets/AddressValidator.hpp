#pragma once
#include <State/Expression.hpp>

#include <QValidator>

namespace State
{
/**
 * @brief The AddressValidator class
 *
 * A QValidator that checks if an address has a correct syntax.
 *
 * For instance :
 *  - "device:/dada/dodo" would work.
 *  - "/;/.f,v ç'tế&'" would not.
 */
class SCORE_LIB_STATE_EXPORT AddressValidator final : public QValidator
{
public:
  virtual ~AddressValidator();
  QValidator::State validate(QString& s, int& pos) const override
  {
    return ::State::Address::validateString(s) ? State::Acceptable : State::Intermediate;
  }
};

class SCORE_LIB_STATE_EXPORT AddressAccessorValidator final : public QValidator
{
public:
  virtual ~AddressAccessorValidator();
  QValidator::State validate(QString& s, int& pos) const override
  {
    auto res = ::State::parseAddressAccessor(s);
    return bool(res) ? State::Acceptable : State::Intermediate;
  }
};

class SCORE_LIB_STATE_EXPORT AddressFragmentValidator : public QValidator
{
public:
  using QValidator::QValidator;
  virtual ~AddressFragmentValidator();
  QValidator::State validate(QString& s, int& pos) const override
  {
    return ::State::Address::validateFragment(s) ? QValidator::State::Acceptable
                                                 : QValidator::State::Invalid;
  }
};
}
