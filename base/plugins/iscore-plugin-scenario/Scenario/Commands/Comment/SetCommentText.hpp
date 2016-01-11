#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <QTextDocument>



namespace Scenario
{
class CommentBlockModel;
namespace Command
{
class SetCommentText final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetCommentText, "Set Text in comment block")
        public:
            SetCommentText(Path<CommentBlockModel> path, QString newComment);

        void undo() const override;
        void redo() const override;

        // SerializableCommand interface
    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<CommentBlockModel> m_path;
        QString m_newComment;
        QString m_oldComment;
};
}
}

