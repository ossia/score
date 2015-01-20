#pragma once
#include <tools/IdentifiedObject.hpp>
#include <tools/SettableIdentifierAlternative.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>
namespace OSSIA
{
	class TimeNode;
}
class State;
class ConstraintModel;
class ScenarioProcessSharedModel;

class EventModel : public NamedObject
{
		Q_OBJECT

		Q_PROPERTY(double heightPercentage
				   READ heightPercentage
				   WRITE setHeightPercentage
				   NOTIFY heightPercentageChanged)

		friend void Visitor<Writer<DataStream>>::writeTo<EventModel>(EventModel& ev);
		friend void Visitor<Writer<JSON>>::writeTo<EventModel>(EventModel& ev);

	public:
		EventModel(QObject* parent);
		EventModel(double yPos, QObject *parent);
		~EventModel();


		template<typename Impl>
		EventModel(Deserializer<Impl>& vis, QObject* parent):
			NamedObject{vis, parent}
		{
			vis.writeTo(*this);
		}

		const QVector<int>& previousConstraints() const;
		const QVector<int>& nextConstraints() const;

		void addNextConstraint(int);
		void addPreviousConstraint(int);

		bool removeNextConstraint(int);
		bool removePreviousConstraint(int);

		void changeTimeNode(int);
		int timeNode() const;

		const std::vector<State*>& states() const;
		void addState(State* s);
		void removeState(int stateId);

		OSSIA::TimeNode* apiObject()
		{ return m_timeEvent;}

		double heightPercentage() const;
		int date() const;

		void translate(int deltaTime);

		void setVerticalExtremity(int, double);
		void eventMovedVertically(double);
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

		void setTopY(double val);

		void setBottomY(double val);


		void setOSSIATimeNode(OSSIA::TimeNode* timeEvent)
		{ m_timeEvent = timeEvent; }


		OSSIA::TimeNode* m_timeEvent{};

		int m_timeNode{};

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

using IdentifiedEventModel = id_mixin<EventModel>;
