#pragma once
#include <tools/IdentifiedObject.hpp>
#include "Document/Constraint/ConstraintModelMetadata.hpp"
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

	public:
		ConstraintModelMetadata metadata;


		friend QDataStream& operator << (QDataStream&, const ConstraintModel&);
		friend QDataStream& operator >> (QDataStream&, ConstraintModel&);

		ConstraintModel(int id, QObject* parent);
		ConstraintModel(int id, double yPos, QObject* parent);
		ConstraintModel(QDataStream&, QObject* parent);
		virtual ~ConstraintModel() = default;

		// Factories for the view models.
		template<typename ViewModelType, typename Arg> // Arg might be an id or a datastream
		ViewModelType* makeViewModel(Arg&& arg, QObject* parent)
		{
			auto viewmodel =  new ViewModelType{arg, this, parent};
			makeViewModel_impl(viewmodel);
			return viewmodel;
		}

		void makeViewModel_impl(AbstractConstraintViewModel* viewmodel);

		// Sub-element creation
		void createProcess(QString processName, int processId);

		/**
		 * @brief createProcess Create a process from a data stream
		 * @param data a data stream containing the name of a process, as given by ProcessSharedModelInterface::processName followed by the data of the process obtained by calling  QDataStream::operator<< on the ProcessSharedModelInterface.
		 * The method saveProcess does this as a convenience.
		 */
		void createProcess(QDataStream& s);
		static void saveProcess(QDataStream& s, ProcessSharedModelInterface* p);

		void removeProcess(int processId);


		void createBox(int boxId);
		void createBox(QDataStream& s);
		void removeBox(int boxId);

		int startEvent();
		int endEvent();
		void setStartEvent(int eventId); // Use ScenarioKey
		void setEndEvent(int eventId); // Use ScenarioKey

		BoxModel* box(int contentId);
		ProcessSharedModelInterface* process(int processId);



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

		int width() const;
		void setWidth(int width);

		double heightPercentage() const;

	signals:
		void processCreated(QString processName, int processId);
		void processRemoved(int processId);

		void boxCreated(int boxId);
		void boxRemoved(int boxId);

		void heightPercentageChanged(double arg);

	public slots:
		void setHeightPercentage(double arg);

	private:
		// TODO maybe put this in public, and let the command call ProcessList and pass a pointer to createProcess (which becomes addProcess)
		void createProcess_impl(ProcessSharedModelInterface*);
		void createBox_impl(BoxModel*);

		OSSIA::TimeBox* m_timeBox{}; // Manages the duration

		std::vector<BoxModel*> m_boxes; // No content -> Phantom ?
		std::vector<ProcessSharedModelInterface*> m_processes;

		int m_startEvent{};
		int m_endEvent{};

		// ___ TEMPORARY ___
		int m_width{200};
		int m_x{};

		double m_heightPercentage{0.5};

};
