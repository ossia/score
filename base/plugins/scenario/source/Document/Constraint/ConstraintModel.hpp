#pragma once
#include <tools/IdentifiedObject.hpp>
#include "Document/Constraint/ConstraintModelMetadata.hpp"
#include <interface/serialization/VisitorInterface.hpp>

#include <QColor>
#include <vector>

namespace OSSIA
{
	class TimeBox;
}

class ProcessSharedModelInterface;
class AbstractConstraintViewModel;

class BoxModel;
class EventModel;
class TimeBox;

/**
 * @brief The ConstraintModel class
 */
// TODO put some of this stuff in the corresponding view models.
class ConstraintModel : public IdentifiedObject
{
		Q_OBJECT

		Q_PROPERTY(double heightPercentage
				   READ heightPercentage
				   WRITE setHeightPercentage
				   NOTIFY heightPercentageChanged)

		// These dates are relative to the beginning of the constraint.
		Q_PROPERTY(int minDuration
				   READ minDuration
				   WRITE setMinDuration
				   NOTIFY minDurationChanged)
		Q_PROPERTY(int maxDuration
				   READ maxDuration
				   WRITE setMaxDuration
				   NOTIFY maxDurationChanged)

	public:
		ConstraintModelMetadata metadata;

		ConstraintModel(int id, QObject* parent);
		ConstraintModel(int id, double yPos, QObject* parent);

		template<typename Impl>
		ConstraintModel(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObject{vis, parent}
		{
			vis.writeTo(*this);
		}

		virtual ~ConstraintModel() = default;

		// Factories for the view models.
		template<typename ViewModelType> // Arg might be an id or a datastream [
		ViewModelType* makeConstraintViewModel(int id, QObject* parent)
		{
			auto viewmodel =  new ViewModelType{id, this, parent};
			setupConstraintViewModel(viewmodel);
			return viewmodel;
		}

		/*
		template<typename ViewModelType, typename Impl> // Arg might be an id or a datastream [
		ViewModelType* makeConstraintViewModel(Deserializer<Impl>& deserializer,
											   QObject* parent)
		{
			auto viewmodel =  new ViewModelType{deserializer, this, parent};
			setupConstraintViewModel(viewmodel);
			return viewmodel;
		}
		*/

		void setupConstraintViewModel(AbstractConstraintViewModel* viewmodel) const;

		// Sub-element creation
		void createProcess(QString processName, int processId);
		void addProcess(ProcessSharedModelInterface*);
		void removeProcess(int processId);

		void createBox(int boxId);
		void addBox(BoxModel*);
		void removeBox(int boxId);

		int startEvent() const;
		int endEvent() const;
		void setStartEvent(int eventId); // Use ScenarioKey
		void setEndEvent(int eventId); // Use ScenarioKey


		BoxModel* box(int contentId) const;
		ProcessSharedModelInterface* process(int processId) const;

		OSSIA::TimeBox* apiObject()
		{ return m_timeBox;}

		// Copies are done because there might be a loop
		// that might change the vector, and invalidate the
		// iterators, leading to a crash quite difficult to debug.
		std::vector<BoxModel*> boxes() const
		{ return m_boxes; }
		std::vector<ProcessSharedModelInterface*> processes() const
		{ return m_processes; }

		int startDate() const;
		void setStartDate(int start);
		void translate(int deltaTime);

		double heightPercentage() const;

		int defaultDuration() const;
		int minDuration() const;
		int maxDuration() const;

	signals:
		void processCreated(QString processName, int processId);
		void processRemoved(int processId);

		void boxCreated(int boxId);
		void boxRemoved(int boxId);

		void heightPercentageChanged(double arg);

        void defaultDurationChanged(int arg);
		void minDurationChanged(int arg);
		void maxDurationChanged(int arg);

	public slots:
		void setHeightPercentage(double arg);

		void setDefaultDuration(int defaultDuration);
		void setMinDuration(int arg);
		void setMaxDuration(int arg);

	private:
		OSSIA::TimeBox* m_timeBox{}; // Manages the duration

		std::vector<BoxModel*> m_boxes; // No content -> Phantom ?
		std::vector<ProcessSharedModelInterface*> m_processes;

		int m_startEvent{};
		int m_endEvent{};

		// ___ TEMPORARY ___
		int m_defaultDuration{200};
		int m_minDuration{m_defaultDuration};
		int m_maxDuration{m_defaultDuration};

		int m_x{}; // origin

		double m_heightPercentage{0.5};

};

