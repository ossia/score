#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class CreateCurvesFromAddresses : public iscore::SerializableCommand
{
	public:
		CreateCurvesFromAddresses();
		CreateCurvesFromAddresses(ObjectPath&& constraint,
								  QStringList addresses);

		virtual void undo() override;
		virtual void redo() override;
		virtual bool mergeWith(const Command* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) const override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_path;
		QStringList m_addresses;

		QVector<QByteArray> m_serializedCommands;
};
