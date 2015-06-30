#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ObjectPath.hpp>


class EventModel;

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, only vertical move is allowed (new state on an existing event)
 */

namespace Scenario
{
    namespace Command
    {

        class MoveNewState : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("MoveNewState", "MoveNewState")
            public:

            MoveNewState(ObjectPath&& scenarioPath,
                    id_type<EventModel> eventId,
                    const double y);

              virtual void undo() override;
              virtual void redo() override;

              void update(const ObjectPath& path,
                  const id_type<EventModel>& id,
                  const double y)
              {
                m_y = y;
              }

              const ObjectPath& path() const
              { return m_path; }

            protected:
              virtual void serializeImpl(QDataStream&) const override;
              virtual void deserializeImpl(QDataStream&) override;

            private:
              ObjectPath m_path;
              id_type<EventModel> m_eventId;
              double m_y;
        };
    }
}
