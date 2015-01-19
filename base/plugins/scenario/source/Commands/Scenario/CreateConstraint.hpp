#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>
#include <QMap>
#include <tuple>

struct EventData;

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
	namespace Command
	{
		/**
 * @brief The CreateEventAfterEventCommand class
 *
 * This Command creates a constraint and another event in a scenario,
 * starting from an event selected by the user.
 */
        class CreateConstraint : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
                CreateConstraint();
                CreateConstraint(ObjectPath&& scenarioPath, int startEvent, int endEvent);
                CreateConstraint& operator=(CreateConstraint&&) = default;

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;

				int m_createdConstraintId{};
                int m_startEventId{};
                int m_endEventId{};

				QMap<std::tuple<int,int,int>, int> m_createdConstraintViewModelIDs;
				int m_createdConstraintFullViewId{};
		};
	}
}
