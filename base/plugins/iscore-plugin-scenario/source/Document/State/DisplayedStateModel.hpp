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
class ConstraintModel;

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

        DisplayedStateModel(const id_type<DisplayedStateModel>& id,
                            const id_type<EventModel>& eventId,
                            double yPos,
                            QObject* parent);

        // TODO berkkk
        void initView(AbstractConstraintView *parentView);

        // Copy
        DisplayedStateModel(const DisplayedStateModel& source,
                            const id_type<DisplayedStateModel>&,
                            QObject* parent);

        // Load
        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        DisplayedStateModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject<DisplayedStateModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        const ScenarioModel* parentScenario() const;

        double heightPercentage() const;

        // TODO put in presenter
        StateView* view() const;

        const iscore::StateList& states() const;
        void replaceStates(const iscore::StateList& newStates);
        void addState(const iscore::State& s);
        void removeState(const iscore::State& s);

        const id_type<EventModel>& eventId() const;

        void setNextConstraint(const id_type<ConstraintModel>&);
        void setPreviousConstraint(const id_type<ConstraintModel>&);

public slots:
        void setHeightPercentage(double y);
        void setPos(qreal y);

    private:
        id_type<EventModel> m_eventId;  // TODO serialize

        // TODO When we shift to id_type = int, put this Optional
        id_type<ConstraintModel> m_previousConstraint;
        id_type<ConstraintModel> m_nextConstraint;

        double m_heightPercentage{0.5}; // In the whole scenario

        iscore::StateList m_states;
        StateView* m_view{};

};

using DisplayedStateList = QList<DisplayedStateModel>;

