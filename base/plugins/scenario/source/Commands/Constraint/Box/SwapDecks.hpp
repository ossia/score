#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class DeckModel;
namespace Scenario
{
    namespace Command
    {
        class SwapDecks : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("SwapDecks", "SwapDecks")
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(SwapDecks, "ScenarioControl")
                SwapDecks(
                    ObjectPath&& box,
                    id_type<DeckModel> first,
                    id_type<DeckModel> second);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_boxPath;
                id_type<DeckModel> m_first, m_second;
        };
    }
}
