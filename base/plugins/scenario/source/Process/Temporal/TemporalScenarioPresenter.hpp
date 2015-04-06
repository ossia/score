#pragma once
#include "ProcessInterface/ProcessPresenterInterface.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <Document/Event/EventData.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>

#include "StateMachines/ScenarioStateMachine.hpp"

namespace iscore
{
    class SerializableCommand;
}
class ProcessViewModelInterface;
class ProcessViewInterface;

class AbstractConstraintViewModel;
class TemporalConstraintViewModel;
class TemporalConstraintPresenter;
class EventPresenter;
class TemporalScenarioViewModel;
class TemporalScenarioView;
class EventModel;
class TimeNodeModel;
class TimeNodePresenter;
class ConstraintModel;
class ScenarioSelectionManager;
class ScenarioViewInterface;
struct EventData;
struct ConstraintData;
class SelectionDispatcher;

class TemporalScenarioPresenter : public ProcessPresenterInterface
{
        Q_OBJECT

        friend class ScenarioStateMachine;
        friend class ScenarioViewInterface;
        friend class ScenarioSelectionManager;

    public:
        TemporalScenarioPresenter(TemporalScenarioViewModel* model,
                                  ProcessViewInterface* view,
                                  QObject* parent);
        virtual ~TemporalScenarioPresenter();


        virtual id_type<ProcessViewModelInterface> viewModelId() const override;
        virtual id_type<ProcessSharedModelInterface> modelId() const override;

        virtual void setWidth(int width) override;
        virtual void setHeight(int height) override;
        virtual void putToFront() override;
        virtual void putBehind() override;

        virtual void parentGeometryChanged() override;

        virtual void on_zoomRatioChanged(ZoomRatio val) override;

        void focus();

        const QList<EventPresenter*>& events() const
        { return m_events; }
        const QList<TimeNodePresenter*>& timeNodes() const
        { return m_timeNodes; }
        const QList<TemporalConstraintPresenter*>& constraints() const
        { return m_constraints; }

        TemporalScenarioView& view() const
        { return *m_view; }
        const ZoomRatio& zoomRatio() const
        { return m_zoomRatio; }

        ScenarioStateMachine& stateMachine()
        { return m_sm; }

    signals:
        void linesExtremityScaled(int, int);

    public slots:
        // Model -> view
        void on_eventCreated(id_type<EventModel> eventId);
        void on_eventDeleted(id_type<EventModel> eventId);

        void on_timeNodeCreated(id_type<TimeNodeModel> timeNodeId);
        void on_timeNodeDeleted(id_type<TimeNodeModel> timeNodeId);

        void on_constraintCreated(id_type<AbstractConstraintViewModel> constraintId);
        void on_constraintViewModelRemoved(id_type<AbstractConstraintViewModel> constraintId);

        void on_askUpdate();

    private slots:
        void addTimeNodeToEvent(id_type<EventModel> eventId, id_type<TimeNodeModel> timeNodeId);

    protected:
        // TODO faire passer l'abstract et utiliser des free functions de cast
        QList<TemporalConstraintPresenter*> m_constraints;
        QList<EventPresenter*> m_events;
        QList<TimeNodePresenter*> m_timeNodes;

        ZoomRatio m_zoomRatio {1};

        TemporalScenarioViewModel* m_viewModel;
        TemporalScenarioView* m_view;

    private:
        void on_eventCreated_impl(EventModel* event_model);
        void on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model);
        void on_timeNodeCreated_impl(TimeNodeModel* timeNode_model);

        ScenarioViewInterface* m_viewInterface{};
        ScenarioStateMachine m_sm;

        FocusDispatcher m_focusDispatcher;
};
