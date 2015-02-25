#pragma once
#include "ProcessInterface/ProcessPresenterInterface.hpp"
#include <tools/SettableIdentifier.hpp>
#include <Document/Event/EventData.hpp>

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
class ScenarioCommandManager;
struct EventData;
struct ConstraintData;

class TemporalScenarioPresenter : public ProcessPresenterInterface
{
	Q_OBJECT

    friend class ScenarioCommandManager;

            Q_PROPERTY(id_type<EventModel> currentlySelectedEvent
				   READ currentlySelectedEvent
				   WRITE setCurrentlySelectedEvent
				   NOTIFY currentlySelectedEventChanged)

	public:
		TemporalScenarioPresenter(ProcessViewModelInterface* model,
								 ProcessViewInterface* view,
								 QObject* parent);
		virtual ~TemporalScenarioPresenter();


		virtual id_type<ProcessViewModelInterface> viewModelId() const;
		virtual id_type<ProcessSharedModelInterface> modelId() const;

		virtual void setWidth(int width) override;
		virtual void setHeight(int height) override;
		virtual void putToFront() override;
		virtual void putBack() override;

		virtual void parentGeometryChanged() override;

		virtual void on_horizontalZoomChanged(int val) override;

		id_type<EventModel> currentlySelectedEvent() const;
		long millisecPerPixel() const;

	signals:
		void currentlySelectedEventChanged(id_type<EventModel> arg);
		void linesExtremityScaled(int, int);        

	public slots:
		// Model -> view
		void on_eventCreated(id_type<EventModel> eventId);
		void on_eventDeleted(id_type<EventModel> eventId);
		void on_eventMoved(id_type<EventModel> eventId);

		void on_timeNodeCreated(id_type<TimeNodeModel> timeNodeId);
        void on_timeNodeDeleted(id_type<TimeNodeModel> timeNodeId);

		void on_constraintCreated(id_type<AbstractConstraintViewModel> constraintId);
		void on_constraintViewModelRemoved(id_type<AbstractConstraintViewModel> constraintId);
		void on_constraintMoved(id_type<ConstraintModel> constraintId);

		// View -> Presenter
		void on_deletePressed();
        void on_clearPressed();

		void on_scenarioPressed();
        void on_scenarioPressedWithControl(QPointF, QPointF);
		void on_scenarioReleased(QPointF, QPointF);

		void on_askUpdate();

	private slots:
		void setCurrentlySelectedEvent(id_type<EventModel> arg);

        void addTimeNodeToEvent(id_type<EventModel> eventId, id_type<TimeNodeModel> timeNodeId);

        void snapEventToTimeNode(EventData* data);

    protected:
        // TODO faire passer l'abstract et utiliser des free functions de cast
        std::vector<TemporalConstraintPresenter*> m_constraints;
        std::vector<EventPresenter*> m_events;
        std::vector<TimeNodePresenter*> m_timeNodes;

        // Necessary for the real-time creation / moving of elements
        bool m_ongoingCommand{};
        int m_ongoingCommandId{-1};

        int m_horizontalZoomSliderVal{};
        double m_millisecPerPixel{1};

        TemporalScenarioViewModel* m_viewModel;
        TemporalScenarioView* m_view;


        id_type<EventModel> m_currentlySelectedEvent{};
        int m_pointedEvent{0};


        EventData m_lastData{};

        QVector<QObject*> m_lastInspectedObjects{};
        QVector<QObject*> m_undoLastInspectedObjects{};

	private:
		void on_eventCreated_impl(EventModel* event_model);
		void on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model);
		void on_timeNodeCreated_impl(TimeNodeModel* timeNode_model);
        void updateTimeNode(id_type<TimeNodeModel> id);


        ScenarioCommandManager* m_cmdManager;


};
