#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class MoveConstraintCommand : public iscore::SerializableCommand
{
	public:
		// TODO le endEvent est-il nécessaire ?
        MoveConstraintCommand(ObjectPath &&scenarioPath, int constraintId, int endEvent, double deltaHeight);
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_scenarioPath;
		int m_constraintId{};
		int m_endEventId{};

//		double m_heightPosition{};
        double m_deltaHeight{};
		double m_oldHeightPosition{};
};
