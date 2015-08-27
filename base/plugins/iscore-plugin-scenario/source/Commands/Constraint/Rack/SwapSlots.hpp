#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class SlotModel;
class RackModel;
namespace Scenario
{
    namespace Command
    {
        class SwapSlots : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("SwapSlots", "SwapSlots")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SwapSlots, "ScenarioControl")
                SwapSlots(
                    Path<RackModel>&& rack,
                    const Id<SlotModel>& first,
                    const Id<SlotModel>& second);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<RackModel> m_rackPath;
                Id<SlotModel> m_first, m_second;
        };
    }
}
