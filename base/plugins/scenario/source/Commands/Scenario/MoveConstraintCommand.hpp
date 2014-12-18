#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

struct ConstraintData;

class MoveConstraintCommand : public iscore::SerializableCommand
{
	public:
		// TODO le endEvent est-il nécessaire ?
		MoveConstraintCommand(ObjectPath &&scenarioPath, ConstraintData d);
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

		double m_oldHeightPosition{};
		double m_newHeightPosition{};
		int m_oldX{};
		int m_newX{};
};
