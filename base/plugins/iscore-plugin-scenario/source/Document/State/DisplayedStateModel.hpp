#pragma once

#include <source/Document/ModelMetadata.hpp>

#include <State/State.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "StateView.hpp"

//class StateView;

class DisplayedStateModel : public IdentifiedObject<DisplayedStateModel>
{
    Q_OBJECT

    private:

        friend void Visitor<Reader<DataStream>>::readFrom<DisplayedStateModel> (const DisplayedStateModel& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<DisplayedStateModel> (const DisplayedStateModel& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<DisplayedStateModel> (DisplayedStateModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<DisplayedStateModel> (DisplayedStateModel& ev);

    public:
        ModelMetadata metadata;

        DisplayedStateModel(id_type<DisplayedStateModel> id,
                            double yPos,
                            StateView *view,
                            QObject* parent);

        // Copy
        DisplayedStateModel(const DisplayedStateModel& copy,
                            const id_type<DisplayedStateModel>&,
                            QObject* parent);

        double heightPercentage() const;

        StateView* view() const;

    public slots:
        void setHeightPercentage(double y);

        const iscore::StateList& states() const;
        void replaceStates(const iscore::StateList& newStates);
        void addState(const iscore::State& s);
        void removeState(const iscore::State& s);

    private:

        double m_heightPercentage{0.5};

        iscore::StateList m_states;
        StateView* m_view;

};

using DisplayedStateList = QList<DisplayedStateModel>;

