#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>


/**
 * @brief The AddProcessToConstraintCommand class
 *
 * For now this command creates a new storey in the current constraintcontentmodel with a new processviewmodel inside
 */
class AddProcessToConstraintCommand : public iscore::SerializableCommand
{
	public:
		AddProcessToConstraintCommand(ObjectPath&& constraintPath, QString process);
		//AddProcessToConstraintCommand(ObjectPath constraintPath, QString process, const char* processSerializedData);
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

		int m_createdProcessId{-1};
		int m_createdStoreyId{-1};
		int m_createdProcessViewModelId{-1};
		int m_contentModelId{};
};
