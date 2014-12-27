#pragma once
#include <tools/IdentifiedObject.hpp>
#include "EventModelSerialization.hpp"
namespace OSSIA
{
	class TimeNode;
}
class State;
class ConstraintModel;
class ScenarioProcessSharedModel;

class EventModel : public IdentifiedObject
{
		Q_OBJECT

		Q_PROPERTY(double heightPercentage
				   READ heightPercentage
				   WRITE setHeightPercentage
				   NOTIFY heightPercentageChanged)

		friend void Visitor<Writer<DataStream>>::visit<EventModel>(EventModel& ev);

	public:
		EventModel(int id, QObject* parent);
		EventModel(int id, double yPos, QObject *parent);


		template<typename Impl>
		EventModel(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObject{vis, parent}
		{
			vis.visit(*this);
		}

		virtual ~EventModel() = default;

		const QVector<int>& previousConstraints() const;
		const QVector<int>& nextConstraints() const;

		void addNextConstraint(int);
		void addPreviousConstraint(int);

		bool removeNextConstraint(int);
		bool removePreviousConstraint(int);

		const std::vector<State*>& states() const;
		void addState(State* s);
		void removeState(int stateId);

		OSSIA::TimeNode* apiObject()
		{ return m_timeNode;}

		double heightPercentage() const;
		int date() const;
		void translate(int deltaTime);
		void setVerticalExtremity(int, double);
		void updateVerticalLink();

		ScenarioProcessSharedModel* parentScenario() const;

		// Should maybe be in the Scenario instead ?
		QMap<int, double> constraintsYPos() const
		{ return m_constraintsYPos; }
		double topY() const
		{ return m_topY; }
		double bottomY() const
		{ return m_bottomY; }


	public slots:
		void setHeightPercentage(double arg);
		void setDate(int date);


	signals:
		void heightPercentageChanged(double arg);
		void messagesChanged();
		void verticalExtremityChanged(double, double);

	private:
		// Setters required for serialization
		void setPreviousConstraints(QVector<int>&& vec)
		{ m_previousConstraints = std::move(vec); }

		void setNextConstraints(QVector<int>&& vec)
		{ m_nextConstraints = std::move(vec); }

		void setConstraintsYPos(QMap<int, double>&& map)
		{ m_constraintsYPos = std::move(map); }

		void setTopY(double val)
		{ m_topY = val; }
		void setBottomY(double val)
		{ m_bottomY = val; }

		void setOSSIATimeNode(OSSIA::TimeNode* timeNode)
		{ m_timeNode = timeNode; }


		OSSIA::TimeNode* m_timeNode{};

		QVector<int> m_previousConstraints;
		QVector<int> m_nextConstraints;
		QMap<int, double> m_constraintsYPos;

		double m_heightPercentage{0.5};

		double m_topY{0.5};
		double m_bottomY{0.5};

		std::vector<State*> m_states;

		/// TEMPORARY. This information has to be queried from OSSIA::Scenario instead.
		int m_date{0}; // Was : m_x

};

