#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

#include <core/tools/ObjectPath.hpp>

/**
 * @brief The EmptyBoxFromConstraint class
 *
 * Remove all the process in the box of a constraint
 */
class EmptyConstraintBoxCommand : public iscore::SerializableCommand
{
	public:
		EmptyConstraintBoxCommand();
		EmptyConstraintBoxCommand(ObjectPath&& constraintPath);
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_path;

		QVector<QByteArray> m_serializedBoxes;
		QVector<QByteArray> m_serializedProcesses;
};
