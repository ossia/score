#pragma once

#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>
#include <QString>
#include <vector>

#include <Process/State/MessageNode.hpp>

class QObject;
namespace State {
struct Address;
}  // namespace iscore

namespace Autom3D
{
class ProcessModel;
class ProcessState final : public ProcessStateDataInterface
{
    public:
        // watchedPoint : something between 0 and 1
        ProcessState(
                ProcessModel& process,
                double watchedPoint,
                QObject* parent);

        QString stateName() const override;
        ProcessModel& process() const;

        ::State::Message message() const;
        double point() const;

        ProcessState* clone(QObject* parent) const override;

        std::vector<State::Address> matchingAddresses() override;
        ::State::MessageList messages() const override;
        ::State::MessageList setMessages(
                const ::State::MessageList&,
                const MessageNode&) override;

    private:
        double m_point{};
};

}
