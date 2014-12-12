#pragma once
#include <tools/IdentifiedObject.hpp>

#include <QColor>
#include <vector>

namespace OSSIA
{
	class TimeBox;
}

class ProcessSharedModelInterface;

class ConstraintContentModel;
class EventModel;
class TimeBox;

/**
 * @brief The ConstraintModel class
 *
 * Contains at least 1 ConstraintContentModel (else nothing can be displayed)
 */
class ConstraintModel : public IdentifiedObject
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

		friend QDataStream& operator << (QDataStream&, const ConstraintModel&);
		friend QDataStream& operator >> (QDataStream&, ConstraintModel&);

		ConstraintModel(int id, QObject* parent);
		ConstraintModel(int id, double yPos, QObject* parent);
		ConstraintModel(QDataStream&, QObject* parent);
		virtual ~ConstraintModel() = default;

		int createProcess(QString processName, int processId);
		int createProcess(QString processName, QDataStream& data);
		void deleteProcess(int processId);

		void createContentModel(int contentModelId);
		void createContentModel(QDataStream& s);
		void deleteContentModel(int viewId);
		void duplicateContentModel(int viewId);

		int startEvent();
		int endEvent();
		void setStartEvent(int eventId); // Use ScenarioKey
		void setEndEvent(int eventId); // Use ScenarioKey

		ConstraintContentModel* contentModel(int contentId);
		ProcessSharedModelInterface* process(int processId);

		QString name() const;
		QString comment() const;
		QColor color() const;


		OSSIA::TimeBox* apiObject()
		{ return m_timeBox;}

		// For the presenter :
		const std::vector<ConstraintContentModel*>& contentModels() const
		{ return m_contentModels; }
		const std::vector<ProcessSharedModelInterface*>& processes() const
		{ return m_processes; }

		double heightPercentage() const;
		int startDate() const;

		int width() const;
		void setWidth(int width);

		int height() const;
		void setHeight(int height);

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
		int createProcess_impl(ProcessSharedModelInterface*);
		void createContentModel_impl(ConstraintContentModel*);

		OSSIA::TimeBox* m_timeBox{}; // Manages the duration

		std::vector<ConstraintContentModel*> m_contentModels; // No content -> Phantom ?
		std::vector<ProcessSharedModelInterface*> m_processes;

		QString m_name{"Constraint."};
		QString m_comment;
		QColor m_color; // Maybe in ContentModel ?
		double m_heightPercentage{0.5}; // Relative y position of the top-left corner. Should maybe be in Scenario ?

		int m_startEvent{};
		int m_endEvent{};
};
