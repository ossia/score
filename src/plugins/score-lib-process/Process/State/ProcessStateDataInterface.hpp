#pragma once
#include <Process/State/MessageNode.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

#include <score_lib_process_export.h>

#include <vector>
#include <verdigris>

namespace Process
{
class ProcessModel;
}
class QObject;

class SCORE_LIB_PROCESS_EXPORT ProcessStateDataInterface
    : public IdentifiedObject<ProcessStateDataInterface>
{
  W_OBJECT(ProcessStateDataInterface)
public:
  ProcessStateDataInterface(Process::ProcessModel& model, QObject* parent);

  virtual ~ProcessStateDataInterface();

  /**
   * @brief matchingAddresses The addresses that correspond to this state.
   *
   * @return nothing if the process doesn't have any "settable" address.
   * Else it returns the addresses that may change.
   */
  virtual std::vector<State::AddressAccessor> matchingAddresses() { return {}; }

  /**
   * @brief messages The current messages in this point of the process.
   */
  virtual State::MessageList messages() const { return {}; }

  /**
   * @brief setMessages Request a message change on behalf of the process.
   *
   * Should return the actual new state of the process.
   *
   */
  virtual State::MessageList
  setMessages(const State::MessageList& newMessages, const Process::MessageNode& currentState)
  {
    return messages();
  }

  Process::ProcessModel& process() const { return m_model; }

public:
  void stateChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, stateChanged)
  /**
   * @brief messagesChanged
   * Sent whenever the messages in the process changed.
   *
   */
  void messagesChanged(const State::MessageList& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, messagesChanged, arg_1)

private:
  Process::ProcessModel& m_model;
};
