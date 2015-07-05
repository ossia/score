#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class QGraphicsObject;
class DisplayedStateModel;
class StateView;
class TemporalScenarioPresenter;
class QMimeData;
class ConstraintModel;

class StatePresenter : public NamedObject
{
        Q_OBJECT

    public:
        StatePresenter(const DisplayedStateModel& model,
                       QGraphicsObject* parentview,
                       QObject* parent);

        virtual ~StatePresenter();

        const id_type<DisplayedStateModel>& id() const;

        StateView* view() const;

        const DisplayedStateModel& model() const;

        bool isSelected() const;

        void handleDrop(const QMimeData* mime);

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void eventHoverEnter();
        void eventHoverLeave();

        void heightPercentageChanged();

    private:
        const DisplayedStateModel& m_model;
        StateView* m_view {};

        CommandDispatcher<> m_dispatcher;
};

