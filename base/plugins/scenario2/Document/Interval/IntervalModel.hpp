#pragma once
#include <QNamedObject>
#include <QColor>
#include <vector>


namespace iscore
{
	class ProcessSharedModelInterface;
}

class IntervalContentModel;

class IntervalModel : public QNamedObject
{
	Q_OBJECT
		Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
		Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)
		Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
	
	public:
		IntervalModel(QObject* parent);
		virtual ~IntervalModel() = default;
		
		void createProcess(QString processName); // TODO void createProcessFromData();
		void deleteProcess(int processId);
		
		void createContentModel();
		void deleteContentModel(int viewId);
		void duplicateContentModel(int viewId);
		
		iscore::ProcessSharedModelInterface* process(int processId);
		
		QString name() const;
		QString comment() const;
		QColor color() const;
		
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
		std::vector<IntervalContentModel*> m_contentModel;
		std::vector<iscore::ProcessSharedModelInterface*> m_processes;
		
		// TODO Mute ? Solo ?
		QString m_name;
		QString m_comment;
		QColor m_color; // Maybe in ContentModel ? 
		int m_intervalId;
		
		int m_nextProcessId{};
		int m_nextContentId{};
};

