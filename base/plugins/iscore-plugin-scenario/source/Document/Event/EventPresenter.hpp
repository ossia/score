#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
class QGraphicsObject;
class EventModel;
class EventView;
class TemporalScenarioPresenter;
class EventPresenter : public NamedObject
{
        Q_OBJECT

    public:
        EventPresenter(const EventModel& model,
                       QGraphicsObject* parentview,
                       QObject* parent);
        virtual ~EventPresenter();

        const id_type<EventModel>& id() const;

        EventView* view() const;
        const EventModel& model() const;

        bool isSelected() const;

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

        void heightPercentageChanged();

    private:
        const EventModel& m_model;
        EventView* m_view {};
};

