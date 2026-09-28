#pragma once

#include <State/Message.hpp>

#include <Process/State/MessageNode.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>

#include <score_plugin_automation_export.h>

#include <optional>
#include <vector>

class QObject;
namespace State
{
struct Address;
} // namespace score

namespace Automation
{
class ProcessModel;
class SCORE_PLUGIN_AUTOMATION_EXPORT ProcessState final : public ProcessStateDataInterface
{
public:
  // watchedPoint : something between 0 and 1
  ProcessState(ProcessModel& process, double watchedPoint, QObject* parent);

  ProcessModel& process() const;

  ::State::Message message() const;
  double point() const;

  std::vector<State::AddressAccessor> matchingAddresses() override;
  std::vector<State::AddressAccessor> takenOverAddresses() const override;
  ::State::MessageList messages() const override;
  ::State::MessageList
  setMessages(const ::State::MessageList&, const Process::MessageNode&) override;

  //! Emits stateChanged (and so messagesChanged) if messages() differs from
  //! what was last announced.
  void updateMessages();

private:
  double m_point{};
  std::optional<::State::MessageList> m_announced;
};
}
