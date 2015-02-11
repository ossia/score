#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class BoxModel;
namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The CopyBox class
		 *
		 * Copy a Box inside the same constraint
		 */
		class CopyBox : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				CopyBox();
				CopyBox(ObjectPath&& boxToCopy);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) const override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_boxPath;

				id_type<BoxModel> m_newBoxId;
		};
	}
}
