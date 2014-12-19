#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>


/**
 * @brief The AddProcessToConstraintCommand class
 *
 * For now this command creates a new storey in the current constraintcontentmodel with a new processviewmodel inside
 */
// TODO Rename in AddProcessToBox.
class AddProcessToConstraintCommand : public iscore::SerializableCommand
{
	public:
		AddProcessToConstraintCommand(ObjectPath&& constraintPath, QString process);

		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_path;
		QString m_processName;

		int m_createdProcessId{};
/*
		int m_createdStoreyId{};
		int m_createdProcessViewModelId{};
		int m_contentModelId{};
*/
};
