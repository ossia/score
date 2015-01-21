#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class State;
namespace Scenario
{
	namespace Command
	{
		class AddStateToEvent : public iscore::SerializableCommand
		{
			public:
				AddStateToEvent();
				AddStateToEvent(ObjectPath&& eventPath, QString message);
				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;
				QString m_message;

				id_type<State> m_stateId{};
		};
	}
}
