#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <Process/TimeValue.hpp>

class CommentBlockModel;

namespace Scenario
{
    namespace  Command
    {
        class MoveCommentBlock final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveCommentBlock, "Move a comment block")
            public:
                MoveCommentBlock(const Path<ScenarioModel>& scenarPath,
                        const Id<CommentBlockModel>& id,
                        const TimeValue& newDate,
                        double newY);

                void update(
                       const Path<ScenarioModel>& scenar,
                       const Id<CommentBlockModel>& id,
                       const TimeValue& newDate,
                       double newYPos)
                {
                    m_newDate = newDate;
                    m_newY = newYPos;
                }
                // Command interface

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<ScenarioModel> m_path;
                Id<CommentBlockModel> m_id;
                TimeValue m_oldDate, m_newDate;
                double m_oldY, m_newY;

        };

    }
}
