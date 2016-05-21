#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <Process/TimeValue.hpp>


namespace Scenario
{
class CommentBlockModel;
namespace  Command
{
class MoveCommentBlock final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MoveCommentBlock, "Move a comment block")
        public:
            MoveCommentBlock(
                const ScenarioModel& scenarPath,
                Id<CommentBlockModel> id,
                TimeValue newDate,
                double newY);

        void update(unused_t, unused_t,
                TimeValue newDate,
                double newYPos)
        {
            m_newDate = std::move(newDate);
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
