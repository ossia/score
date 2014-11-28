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
class IntervalModel : public QIdentifiedObject
{
	Q_OBJECT
		Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
		Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)
		Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

	public:
		friend QDataStream& operator << (QDataStream&, const IntervalModel&);

		IntervalModel(int id, QObject* parent);
		IntervalModel(QDataStream&, QObject* parent);
		virtual ~IntervalModel() = default;

		int createProcess(QString processName);
		int createProcess(QString processName, QDataStream& data);
		void deleteProcess(int processId);

		void createContentModel();
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

	public slots:
		void setName(QString arg);
		void setComment(QString arg);
		void setColor(QColor arg);

	signals:
		void processCreated(QString processName, int processId);
		void processDeleted(int processId);

		void viewCreated(int viewId);
		void viewDeleted(int viewId);

		void nameChanged(QString arg);
		void commentChanged(QString arg);
		void colorChanged(QColor arg);

	private:
		OSSIA::TimeBox* m_timeBox{}; // Manages the duration

		std::vector<IntervalContentModel*> m_contentModels; // No content -> Phantom ?
		std::vector<iscore::ProcessSharedModelInterface*> m_processes;

		// TODO Mute ? Solo ?
		QString m_name;
		QString m_comment;
		QColor m_color; // Maybe in ContentModel ?

		int m_nextProcessId{};
		int m_nextContentId{};

		int m_startEvent{};
		int m_endEvent{};
};
