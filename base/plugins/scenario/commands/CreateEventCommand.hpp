#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <QPointF>
class CreatEventCommand : public iscore::SerializableCommand
{
	public:
		CreatEventCommand(int modelId, QPointF position);
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;
		
	private:
		int m_modelId{};
		QPointF m_position{};
};
