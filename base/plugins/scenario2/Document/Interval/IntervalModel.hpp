#pragma once
#include <QNamedObject>
#include <QColor>
#include <vector>

namespace OSSIA
{
	class TimeBox;
}
namespace iscore
{
	class ProcessSharedModelInterface;
}

class IntervalContentModel;
class EventModel;
class TimeBox;
/**
 * @brief The IntervalModel class
 *
 * Contains at least 1 IntervalContentModel (else nothing can be displayed)
 */
class IntervalModel : public QIdentifiedObject
{
	Q_OBJECT
		Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
		Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)
		Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
		Q_PROPERTY(double heightPercentage READ heightPercentage WRITE setHeightPercentage NOTIFY heightPercentageChanged)

	public:

		// ___ TEMPORARY ___
		int m_width{200};
		int m_height{200};
		int m_x{};

		friend QDataStream& operator << (QDataStream&, const IntervalModel&);

		IntervalModel(int id, QObject* parent);
        IntervalModel(int id, double yPos, QObject* parent);
		IntervalModel(QDataStream&, QObject* parent);
		virtual ~IntervalModel() = default;

		int createProcess(QString processName);
		int createProcess(QString processName, QDataStream& data);
		void deleteProcess(int processId);

		void createContentModel();
		void createContentModel(QDataStream& s);
		void deleteContentModel(int viewId);
		void duplicateContentModel(int viewId);

		int startEvent();
		int endEvent();
		void setStartEvent(int eventId); // Use ScenarioKey
		void setEndEvent(int eventId); // Use ScenarioKey

		IntervalContentModel* contentModel(int contentId);
		iscore::ProcessSharedModelInterface* process(int processId);

		QString name() const;
		QString comment() const;
		QColor color() const;


		OSSIA::TimeBox* apiObject()
		{ return m_timeBox;}

		// For the presenter :
		const std::vector<IntervalContentModel*>& contentModels() const
		{ return m_contentModels; }
		const std::vector<iscore::ProcessSharedModelInterface*>& processes() const
		{ return m_processes; }
		
		double heightPercentage() const;

	public slots:
		void setName(QString arg);
		void setComment(QString arg);
		void setColor(QColor arg);
		void setHeightPercentage(double arg);

	signals:
		void processCreated(QString processName, int processId);
		void processDeleted(int processId);

		void viewCreated(int viewId);
		void viewDeleted(int viewId);

		void nameChanged(QString arg);
		void commentChanged(QString arg);
		void colorChanged(QColor arg);
		void heightPercentageChanged(double arg);

	private:
		int createProcess_impl(iscore::ProcessSharedModelInterface*);
		void createContentModel_impl(IntervalContentModel*);

		OSSIA::TimeBox* m_timeBox{}; // Manages the duration

		std::vector<IntervalContentModel*> m_contentModels; // No content -> Phantom ?
		std::vector<iscore::ProcessSharedModelInterface*> m_processes;

		QString m_name{"Interval."};
		QString m_comment;
		QColor m_color; // Maybe in ContentModel ?
		double m_heightPercentage{0.5}; // Relative y position of the top-left corner. Should maybe be in Scenario ?

		int m_startEvent{};
		int m_endEvent{};
};
