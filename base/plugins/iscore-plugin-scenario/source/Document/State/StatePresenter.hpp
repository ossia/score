#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class QGraphicsObject;
class StateModel;
class StateView;
class TemporalScenarioPresenter;
class QMimeData;
class ConstraintModel;

class StatePresenter : public NamedObject
{
        Q_OBJECT

    public:
        StatePresenter(const StateModel& model,
                       QGraphicsObject* parentview,
                       QObject* parent);

        virtual ~StatePresenter();

        const id_type<StateModel>& id() const;

        StateView* view() const;

        const StateModel& model() const;

        bool isSelected() const;

        void handleDrop(const QMimeData* mime);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

    private:
        const StateModel& m_model;
        StateView* m_view {};

        CommandDispatcher<> m_dispatcher;
};

