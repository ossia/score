#pragma once
#include "ProcessInterface/ProcessPresenterInterface.hpp"
#include <tools/SettableIdentifier.hpp>

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
class TemporalScenarioProcessViewModel;
class TemporalScenarioProcessView;
class EventModel;
class TimeNodeModel;
class TimeNodePresenter;
class ConstraintModel;
struct EventData;
struct ConstraintData;

class TemporalScenarioProcessPresenter : public ProcessPresenterInterface
{
	Q_OBJECT

		Q_PROPERTY(id_type<EventModel> currentlySelectedEvent
				   READ currentlySelectedEvent
				   WRITE setCurrentlySelectedEvent
				   NOTIFY currentlySelectedEventChanged)

	public:
		TemporalScenarioProcessPresenter(ProcessViewModelInterface* model,
								 ProcessViewInterface* view,
								 QObject* parent);
		virtual ~TemporalScenarioProcessPresenter();


		virtual id_type<ProcessViewModelInterface> viewModelId() const;
		virtual id_type<ProcessSharedModelInterface> modelId() const;

		virtual void putToFront() override;
		virtual void putBack() override;

		void on_horizontalZoomChanged(int val);

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

        void clearContentFromSelection();
        void deleteSelection();

	private slots:
		void setCurrentlySelectedEvent(id_type<EventModel> arg);
		void createConstraint(EventData data);
		void moveEventAndConstraint(EventData data);
		void moveConstraint(ConstraintData data);

	private:
		void on_eventCreated_impl(EventModel* event_model);
		void on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model);
		void on_timeNodeCreated_impl(TimeNodeModel* timeNode_model);
        void updateTimeNode(id_type<TimeNodeModel> id);


		TemporalScenarioProcessViewModel* m_viewModel;
		TemporalScenarioProcessView* m_view;

		std::vector<TemporalConstraintPresenter*> m_constraints;
		std::vector<EventPresenter*> m_events;
		std::vector<TimeNodePresenter*> m_timeNodes;

		id_type<EventModel> m_currentlySelectedEvent{};
		int m_pointedEvent{0};

		long m_millisecPerPixel{1};
};
