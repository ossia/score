#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class SlotModel;
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
                    ObjectPath&& rack,
                    id_type<SlotModel> first,
                    id_type<SlotModel> second);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_rackPath;
                id_type<SlotModel> m_first, m_second;
        };
    }
}
