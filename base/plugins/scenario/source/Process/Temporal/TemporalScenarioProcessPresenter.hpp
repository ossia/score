#pragma once
#include "ProcessInterface/ProcessPresenterInterface.hpp"
#include <tools/SettableIdentifierAlternative.hpp>

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
using IdentifiedEventModel = id_mixin<EventModel>;
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


		virtual int viewModelId() const;
		virtual int modelId() const;

		virtual void putToFront() override;
		virtual void putBack() override;

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
		void setCurrentlySelectedEvent(id_type<EventModel> arg);
		void createConstraint(EventData data);
		void moveEventAndConstraint(EventData data);
		void moveConstraint(ConstraintData data);

	private:
		void on_eventCreated_impl(IdentifiedEventModel* event_model);
		void on_constraintCreated_impl(TemporalConstraintViewModel* constraint_view_model);
		void on_timeNodeCreated_impl(TimeNodeModel* timeNode_model);


		TemporalScenarioProcessViewModel* m_viewModel;
		TemporalScenarioProcessView* m_view;

		std::vector<TemporalConstraintPresenter*> m_constraints;
		std::vector<EventPresenter*> m_events;
		std::vector<TimeNodePresenter*> m_timeNodes;

		id_type<EventModel> m_currentlySelectedEvent{};
		int m_pointedEvent{0};
		long m_millisecPerPixel{1};
};
