#pragma once
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <qpoint.h>

class QGraphicsItem;
class QMimeData;
class QObject;
class StateModel;
class StateView;
template <typename tag, typename impl> class id_base_t;

class StatePresenter final : public NamedObject
{
        Q_OBJECT

    public:
        StatePresenter(const StateModel& model,
                       QGraphicsItem* parentview,
                       QObject* parent);

        virtual ~StatePresenter();

        const Id<StateModel>& id() const;

        StateView* view() const;

        const StateModel& model() const;

        bool isSelected() const;

        void handleDrop(const QMimeData* mime);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void hoverEnter();
        void hoverLeave();

    private:
        void updateStateView();

        const StateModel& m_model;
        StateView* m_view {};

        CommandDispatcher<> m_dispatcher;
};

