#pragma once
#include <Process/State/ProcessStateDataInterface.hpp>
#include <QString>
#include <vector>

#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>
#include <iscore_lib_dummyprocess_export.h>


namespace Process { class ProcessModel; }
class QObject;
namespace State {
struct Address;
}  // namespace iscore

namespace Dummy
{
class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyState final : public ProcessStateDataInterface
{
    public:
        DummyState(Process::ProcessModel& model, QObject* parent);
        QString stateName() const override;
        ProcessStateDataInterface* clone(QObject* parent) const override;

        std::vector<State::Address> matchingAddresses() override;
        State::MessageList messages() const override;
        State::MessageList setMessages(
                const State::MessageList& newMessages,
                const Process::MessageNode& currentState) override;
};
}
