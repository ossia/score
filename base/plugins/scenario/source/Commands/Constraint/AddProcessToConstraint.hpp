#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
	namespace Command
	{
		/**
 * @brief The AddProcessToConstraintCommand class
 *
 * For now this command creates a new deck in the current constraintcontentmodel with a new processviewmodel inside
 */
		class AddProcessToConstraint : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				AddProcessToConstraint();
				AddProcessToConstraint(ObjectPath&& constraintPath, QString process);

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
		};
	}
}
