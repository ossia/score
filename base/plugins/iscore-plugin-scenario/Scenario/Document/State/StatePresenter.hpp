#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class QGraphicsItem;
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

