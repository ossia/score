#pragma once
#include <Process/State/MessageNode.hpp>
#include <QString>
#include <State/Message.hpp>
#include <score/tools/std/Optional.hpp>
#include <vector>

#include <State/Address.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score_lib_process_export.h>

namespace Process
{
class ProcessModel;
}
class QObject;

class SCORE_LIB_PROCESS_EXPORT ProcessStateDataInterface
    : public IdentifiedObject<ProcessStateDataInterface>
{
  Q_OBJECT
public:
  ProcessStateDataInterface(Process::ProcessModel& model, QObject* parent);

  virtual ~ProcessStateDataInterface();

  /**
   * @brief matchingAddresses The addresses that correspond to this state.
   *
   * @return nothing if the process doesn't have any "settable" address.
   * Else it returns the addresses that may change.
   */
  virtual std::vector<State::AddressAccessor> matchingAddresses()
  {
    return {};
  }

  /**
   * @brief messages The current messages in this point of the process.
   */
  virtual State::MessageList messages() const
  {
    return {};
  }

  /**
   * @brief setMessages Request a message change on behalf of the process.
   *
   * Should return the actual new state of the process.
   *
   */
  virtual State::MessageList setMessages(
      const State::MessageList& newMessages,
      const Process::MessageNode& currentState)
  {
    return messages();
  }

  Process::ProcessModel& process() const
  {
    return m_model;
  }

Q_SIGNALS:
  void stateChanged();
  /**
   * @brief messagesChanged
   * Sent whenever the messages in the process changed.
   *
   */
  void messagesChanged(const State::MessageList&);

private:
  Process::ProcessModel& m_model;
};
