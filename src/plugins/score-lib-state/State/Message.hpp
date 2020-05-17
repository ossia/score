#pragma once
#include <State/Address.hpp>
#include <State/Value.hpp>
namespace State
{
/**
 * @brief The Message struct
 *
 * A message is an Address associated with a value :
 *
 *  aDevice:/aNode/anotherNode 2345
 *
 */
struct SCORE_LIB_STATE_EXPORT Message
{
  bool operator==(const Message& m) const noexcept
  {
    return address == m.address && value == m.value;
  }

  bool operator!=(const Message& m) const noexcept
  {
    return address != m.address || value != m.value;
  }

  // bool operator<(const Message& m) const noexcept { return
  // address.toString() < m.address.toString(); }

  QString toString() const;

  AddressAccessor address;
  Value value;
};

SCORE_LIB_STATE_EXPORT
QDebug operator<<(QDebug s, const Message& mess);

using MessageList = QList<Message>;
inline bool operator<(const State::MessageList&, const State::MessageList&)
{
  return false;
}
}

Q_DECLARE_METATYPE(State::Message)
Q_DECLARE_METATYPE(State::MessageList)
W_REGISTER_ARGTYPE(State::Message)
W_REGISTER_ARGTYPE(State::MessageList)
