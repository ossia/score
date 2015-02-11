#pragma once
#include <tools/IdentifiedObject.hpp>
#include <tools/SettableIdentifier.hpp>
#include "Document/ModelMetadata.hpp"
#include <interface/serialization/VisitorInterface.hpp>
#include "ProcessInterface/TimeValue.hpp"

#include <QColor>
#include <vector>

namespace OSSIA
{
	class TimeBox;
}

class ProcessSharedModelInterface;
class AbstractConstraintViewModel;
class FullViewConstraintViewModel;

class BoxModel;
class EventModel;
class TimeBox;

/**
 * @brief The ConstraintModel class
 */
// TODO put some of this stuff in the corresponding view models.
class ConstraintModel : public IdentifiedObject<ConstraintModel>
{
		Q_OBJECT

		Q_PROPERTY(double heightPercentage
				   READ heightPercentage
				   WRITE setHeightPercentage
				   NOTIFY heightPercentageChanged)

		// These dates are relative to the beginning of the constraint.
		Q_PROPERTY(TimeValue minDuration
				   READ minDuration
				   WRITE setMinDuration
				   NOTIFY minDurationChanged)
		Q_PROPERTY(TimeValue maxDuration
				   READ maxDuration
				   WRITE setMaxDuration
				   NOTIFY maxDurationChanged)

	public:
        ModelMetadata metadata;

		ConstraintModel(id_type<ConstraintModel>,
						id_type<AbstractConstraintViewModel> fullViewId,
						QObject* parent);
		ConstraintModel(id_type<ConstraintModel>,
						id_type<AbstractConstraintViewModel> fullViewId,
						double yPos,
						QObject* parent);

		// Copy
		ConstraintModel(ConstraintModel* source,
						id_type<ConstraintModel> id,
						QObject* parent);
		~ConstraintModel();

		template<typename DeserializerVisitor>
		ConstraintModel(DeserializerVisitor&& vis, QObject* parent):
			IdentifiedObject<ConstraintModel>{vis, parent}
		{
			vis.writeTo(*this);
		}

		// Factories for the view models.
		template<typename ViewModelType> // Arg might be an id or a datastream [
		ViewModelType* makeConstraintViewModel(id_type<AbstractConstraintViewModel> id, QObject* parent)
		{
			auto viewmodel =  new ViewModelType{id, this, parent};
			setupConstraintViewModel(viewmodel);
			return viewmodel;
		}

		// Note : the Constraint does not have ownership (it's generally the Deck)
		void setupConstraintViewModel(AbstractConstraintViewModel* viewmodel);

		// Sub-element creation
		void addProcess(ProcessSharedModelInterface*);
		void removeProcess(id_type<ProcessSharedModelInterface> processId);

		void createBox(id_type<BoxModel> boxId);
		void addBox(BoxModel*);
		void removeBox(id_type<BoxModel> boxId);

		id_type<EventModel> startEvent() const;
		id_type<EventModel> endEvent() const;
		void setStartEvent(id_type<EventModel> eventId); // Use ScenarioKey
		void setEndEvent(id_type<EventModel> eventId); // Use ScenarioKey


		BoxModel* box(id_type<BoxModel> id) const;
		ProcessSharedModelInterface* process(id_type<ProcessSharedModelInterface> processId) const;

		OSSIA::TimeBox* apiObject()
		{ return m_timeBox;}

		// Copies are done because there might be a loop
		// that might change the vector, and invalidate the
		// iterators, leading to a crash quite difficult to debug.
		std::vector<BoxModel*> boxes() const
		{ return m_boxes; }
		std::vector<ProcessSharedModelInterface*> processes() const
		{ return m_processes; }

		const QVector<AbstractConstraintViewModel*>& viewModels() const
		{ return m_constraintViewModels; }

		TimeValue startDate() const;
		void setStartDate(TimeValue start);
		void translate(TimeValue deltaTime);
		void translate(int msec);

		double heightPercentage() const;

		TimeValue defaultDuration() const;
		TimeValue minDuration() const;
		TimeValue maxDuration() const;

		FullViewConstraintViewModel* fullView() const
		{ return m_fullViewModel; }

		void setFullView(FullViewConstraintViewModel* fv);

	signals:
		void processCreated(QString processName, id_type<ProcessSharedModelInterface> processId);
		void processRemoved(id_type<ProcessSharedModelInterface> processId);

		void boxCreated(id_type<BoxModel> boxId);
		void boxRemoved(id_type<BoxModel> boxId);

		void heightPercentageChanged(double arg);

		void defaultDurationChanged(TimeValue arg);
		void minDurationChanged(TimeValue arg);
		void maxDurationChanged(TimeValue arg);

	public slots:
		void setHeightPercentage(double arg);

		void setDefaultDuration(TimeValue defaultDuration);
		void setMinDuration(TimeValue arg);
		void setMaxDuration(TimeValue arg);

		// To remove. In msecs.
		void setStartDate(int arg);
		void setDefaultDuration(int defaultDuration);
		void setMinDuration(int arg);
		void setMaxDuration(int arg);

	private slots:
		void on_destroyedViewModel(QObject*);

	private:
		OSSIA::TimeBox* m_timeBox{}; // Manages the duration

		std::vector<BoxModel*> m_boxes; // No content -> Phantom ?
		std::vector<ProcessSharedModelInterface*> m_processes;

		// The small view constraint view models that show this constraint
		// The constraint does not have ownership of these: their parent (in the Qt sense) are
		// the scenario view models
		QVector<AbstractConstraintViewModel*> m_constraintViewModels;

		// Model for the full view. It's always a Temporal one (but it could be specialized, in order to provide the extensibility, maybe ?)
		FullViewConstraintViewModel* m_fullViewModel{};

		id_type<EventModel> m_startEvent{};
		id_type<EventModel> m_endEvent{};

		// ___ Use TimeValue instead ___
		TimeValue m_defaultDuration{0, 2, 30}; // 2 minutes
		TimeValue m_minDuration{m_defaultDuration};
		TimeValue m_maxDuration{m_defaultDuration};

		TimeValue m_x{}; // origin

		double m_heightPercentage{0.5};
};
