#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class SlotModel;
class RackModel;
namespace Scenario
{
    namespace Command
    {
        class SwapSlots final : public iscore::SerializableCommand
        {
                ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), SwapSlots, "SwapSlots")
            public:
                SwapSlots(
                    Path<RackModel>&& rack,
                    const Id<SlotModel>& first,
                    const Id<SlotModel>& second);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<RackModel> m_rackPath;
                Id<SlotModel> m_first, m_second;
        };
    }
}
