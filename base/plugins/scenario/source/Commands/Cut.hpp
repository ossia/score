#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        class Cut : public iscore::SerializableCommand
        {
            public:
                virtual void undo() override;
                virtual void redo() override;
                virtual int id() const override;
                virtual bool mergeWith (const QUndoCommand* other) override;

            protected:
                virtual void serializeImpl (QDataStream&) const override;
                virtual void deserializeImpl (QDataStream&) override;
        };
    }
}
