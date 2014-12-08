#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>
#include <QString>


/**
 * @brief The AddProcessToIntervalCommand class
 *
 * For now this command creates a new storey in the current intervalcontentmodel with a new processviewmodel inside
 */
class AddProcessToIntervalCommand : public iscore::SerializableCommand
{
	public:
		AddProcessToIntervalCommand(ObjectPath&& intervalPath, QString process);
		//AddProcessToIntervalCommand(ObjectPath intervalPath, QString process, const char* processSerializedData);
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
};
