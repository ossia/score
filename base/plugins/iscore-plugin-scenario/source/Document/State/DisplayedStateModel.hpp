#pragma once

#include <source/Document/ModelMetadata.hpp>

#include <State/State.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>

#include "StateView.hpp"

class AbstractConstraintView;
class ScenarioModel;
class EventModel;

class DisplayedStateModel : public IdentifiedObject<DisplayedStateModel>
{
    Q_OBJECT

    private:

        friend void Visitor<Reader<DataStream>>::readFrom<DisplayedStateModel> (const DisplayedStateModel& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<DisplayedStateModel> (const DisplayedStateModel& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<DisplayedStateModel> (DisplayedStateModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<DisplayedStateModel> (DisplayedStateModel& ev);

    public:
        Selectable selection; // TODO state connections
        ModelMetadata metadata; // TODO : usefull ?

        DisplayedStateModel(id_type<DisplayedStateModel> id,
                            id_type<EventModel> eventId,
                            double yPos,
                            QObject* parent);

        void initView(AbstractConstraintView *parentView);

        // Copy
        DisplayedStateModel(const DisplayedStateModel& copy,
                            const id_type<DisplayedStateModel>&,
                            QObject* parent);

        const ScenarioModel* parentScenario() const;

        double heightPercentage() const;

        StateView* view() const;

        const iscore::StateList& states() const;
        void replaceStates(const iscore::StateList& newStates);
        void addState(const iscore::State& s);
        void removeState(const iscore::State& s);

        const id_type<EventModel>& eventId() const;

public slots:
        void setHeightPercentage(double y);
        void setPos(qreal y);

    private:

        id_type<EventModel> m_eventId;  // TODO serialize
        double m_heightPercentage{0.5}; // Usefull ? Not sure because it moves with his parent, the constraint ...

        iscore::StateList m_states;
        StateView* m_view{};

};

using DisplayedStateList = QList<DisplayedStateModel>;

