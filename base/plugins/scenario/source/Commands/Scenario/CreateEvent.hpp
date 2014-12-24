#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include "Commands/Scenario/CreateEventAfterEvent.hpp"
#include <tools/ObjectPath.hpp>

#include <QPointF>

struct EventModelData;

class CreateEventTest;
class HideBoxInViewModelTest;
class MoveEventTest;

namespace Scenario
{
	namespace Command
	{
		/**
 * @brief The CreateEventCommand class
 *
 * This command creates an Event, which is linked to the first event in the
 * scenario.
 */
		class CreateEvent : public iscore::SerializableCommand
		{
				friend class ::CreateEventTest;
				friend class ::MoveEventTest;
				friend class ::HideBoxInViewModelTest;

			public:
				CreateEvent();
				~CreateEvent();
				CreateEvent(ObjectPath&& scenarioPath, int time, double heightPosition);
				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				CreateEventAfterEvent* m_cmd{};
		};
	}
}