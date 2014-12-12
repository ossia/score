#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class MoveIntervalCommand : public iscore::SerializableCommand
{
	public:
		// TODO le endEvent est-il nécessaire ?
		MoveIntervalCommand(ObjectPath &&scenarioPath, int intervalId, int endEvent, double heightPosition);
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_scenarioPath;
		int m_intervalId{};
		int m_endEventId{};

		double m_heightPosition{};

		double m_oldHeightPosition{};
};
