#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

struct ConstraintData;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
	namespace Command
	{
		class MoveConstraint : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				MoveConstraint();
				// TODO le endEvent est-il nécessaire ?
				MoveConstraint(ObjectPath &&scenarioPath, ConstraintData d);
				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;
				int m_constraintId{};

				double m_oldHeightPosition{};
				double m_newHeightPosition{};
				int m_oldX{};
				int m_newX{};
		};
	}
}