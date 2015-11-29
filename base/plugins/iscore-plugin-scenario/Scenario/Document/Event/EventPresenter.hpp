#pragma once
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <qpoint.h>
#include <qstring.h>

class EventModel;
class EventView;
class QGraphicsObject;
class QMimeData;
class QObject;
template <typename tag, typename impl> class id_base_t;

class EventPresenter final : public NamedObject
{
        Q_OBJECT

    public:
        EventPresenter(const EventModel& model,
                       QGraphicsObject* parentview,
                       QObject* parent);
        virtual ~EventPresenter();

        const Id<EventModel>& id() const;

        EventView* view() const;
        const EventModel& model() const;

        bool isSelected() const;

        void handleDrop(const QPointF& pos, const QMimeData* mime);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

    private slots:
        void triggerSetted(QString);

    private:
        const EventModel& m_model;
        EventView* m_view {};

        CommandDispatcher<> m_dispatcher;
};

