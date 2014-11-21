#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

class DeleteEventCommand : public iscore::SerializableCommand
{
	public:
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;
};
