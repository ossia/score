#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <QPointF>
#include <tools/ObjectPath.hpp>

class CreateEventCommand : public iscore::SerializableCommand
{
	public:
		CreateEventCommand();
		CreateEventCommand(ObjectPath&& scenarioPath, int time, float heightPosition);
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_path;

		int m_intervalId{};
		int m_time{};
		float m_heightPosition{};
};
