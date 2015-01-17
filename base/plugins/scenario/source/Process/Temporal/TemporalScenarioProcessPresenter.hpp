#pragma once
#include "ProcessInterface/ProcessPresenterInterface.hpp"

namespace iscore
{
	class SerializableCommand;
}
class ProcessViewModelInterface;
class ProcessViewInterface;

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

		Q_PROPERTY(int currentlySelectedEvent
				   READ currentlySelectedEvent
				   WRITE setCurrentlySelectedEvent
				   NOTIFY currentlySelectedEventChanged)

	public:
		TemporalScenarioProcessPresenter(ProcessViewModelInterface* model,
								 ProcessViewInterface* view,
								 QObject* parent);
		virtual ~TemporalScenarioProcessPresenter();


		virtual int viewModelId() const;
		virtual int modelId() const;

		virtual void putToFront() override;
		virtual void putBack() override;

		int currentlySelectedEvent() const;
        long millisecPerPixel() const;

	signals:
		void currentlySelectedEventChanged(int arg);
		void linesExtremityScaled(int, int);

	public slots:
		// Model -> view
		void on_eventCreated(int eventId);
		void on_eventDeleted(int eventId);
		void on_eventMoved(int eventId);

        void on_timeNodeCreated(int timeNodeId);

		void on_constraintCreated(int constraintId);
		void on_constraintViewModelRemoved(int constraintId);
		void on_constraintMoved(int constraintId);

		// View -> Presenter
		void on_deletePressed();

		void on_scenarioPressed();
		void on_scenarioPressedWithControl(QPointF);
        void on_scenarioReleased(QPointF, QPointF);

		void on_askUpdate();

		void deleteSelection();

	private slots:
		void setCurrentlySelectedEvent(int arg);
        void createConstraint(EventData data);
		void moveEventAndConstraint(EventData data);
		void moveConstraint(ConstraintData data);

	private:
		void on_eventCreated_impl(EventModel* event_model);
		void on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model);
        void on_timeNodeCreated_impl(TimeNodeModel* timeNode_model);


		TemporalScenarioProcessViewModel* m_viewModel;
		TemporalScenarioProcessView* m_view;

		std::vector<TemporalConstraintPresenter*> m_constraints;
		std::vector<EventPresenter*> m_events;
        std::vector<TimeNodePresenter*> m_timeNodes;

		int m_currentlySelectedEvent{};
        int m_pointedEvent{0};
        long m_millisecPerPixel{1};
};
