#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include "Document/Event/EventData.hpp"

class EventModel;
class EventView;

class EventPresenter : public NamedObject
{
        Q_OBJECT

    public:
        EventPresenter(EventModel* model, EventView* view, QObject* parent);
        virtual ~EventPresenter();

        id_type<EventModel> id() const;

        EventView* view() const;
        EventModel* model() const;

        bool isSelected() const;

    signals:
        void eventHoverEnter();
        void eventHoverLeave();

        void linesExtremityChange(int, double);

    private:
        EventModel* m_model {};
        EventView* m_view {};
};

