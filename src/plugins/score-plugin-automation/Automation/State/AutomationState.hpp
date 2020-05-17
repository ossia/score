#pragma once

#include <Process/State/MessageNode.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>

#include <vector>

class QObject;
namespace State
{
struct Address;
} // namespace score

namespace Automation
{
class ProcessModel;
class ProcessState final : public ProcessStateDataInterface
{
public:
  // watchedPoint : something between 0 and 1
  ProcessState(ProcessModel& process, double watchedPoint, QObject* parent);

  ProcessModel& process() const;

  ::State::Message message() const;
  double point() const;

  std::vector<State::AddressAccessor> matchingAddresses() override;
  ::State::MessageList messages() const override;
  ::State::MessageList
  setMessages(const ::State::MessageList&, const Process::MessageNode&) override;

private:
  double m_point{};
};
}
