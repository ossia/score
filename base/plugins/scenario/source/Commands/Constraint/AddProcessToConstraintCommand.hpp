#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>



class AddProcessViewToDeckTest;

/**
 * @brief The AddProcessToConstraintCommand class
 *
 * For now this command creates a new deck in the current constraintcontentmodel with a new processviewmodel inside
 */
class AddProcessToConstraintCommand : public iscore::SerializableCommand
{
		friend class ::AddProcessViewToDeckTest;
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
		int m_createdDeckId{};
		int m_createdProcessViewModelId{};
		int m_contentModelId{};
*/
};
