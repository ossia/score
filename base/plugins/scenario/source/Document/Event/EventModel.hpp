#pragma once
#include <tools/IdentifiedObject.hpp>
#include <tools/SettableIdentifierAlternative.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>

#include <unordered_map>
namespace OSSIA
{
	class TimeNode;
}
class State;
class ConstraintModel;
class TimeNodeModel;
class ScenarioProcessSharedModel;

class EventModel : public IdentifiedObject<EventModel>
{
		Q_OBJECT

		Q_PROPERTY(double heightPercentage
				   READ heightPercentage
				   WRITE setHeightPercentage
				   NOTIFY heightPercentageChanged)

		friend void Visitor<Writer<DataStream>>::writeTo<EventModel>(EventModel& ev);
		friend void Visitor<Writer<JSON>>::writeTo<EventModel>(EventModel& ev);

	public:
		EventModel(id_type<EventModel>, QObject* parent);
		EventModel(id_type<EventModel>, double yPos, QObject *parent);
		~EventModel();


		template<typename Impl>
		EventModel(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObject<EventModel>{vis, parent}
		{
			vis.writeTo(*this);
		}

		const QVector<id_type<ConstraintModel> >& previousConstraints() const;
		const QVector<id_type<ConstraintModel>>& nextConstraints() const;

		void addNextConstraint(id_type<ConstraintModel>);
		void addPreviousConstraint(id_type<ConstraintModel>);

		bool removeNextConstraint(id_type<ConstraintModel>);
		bool removePreviousConstraint(id_type<ConstraintModel>);

		void changeTimeNode(id_type<TimeNodeModel>);
		id_type<TimeNodeModel> timeNode() const;

		const std::vector<State*>& states() const;
		void addState(State* s);
		void removeState(id_type<State> stateId);

		OSSIA::TimeNode* apiObject()
		{ return m_timeEvent;}

		double heightPercentage() const;
		int date() const;

		void translate(int deltaTime);

		void setVerticalExtremity(id_type<ConstraintModel>, double);
		void eventMovedVertically(double);

		ScenarioProcessSharedModel* parentScenario() const;

		// Should maybe be in the Scenario instead ?
		std::unordered_map<id_type<ConstraintModel>,
						   double,
						   id_hash<ConstraintModel>> constraintsYPos() const
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

	private:
		// Setters required for serialization
		void setPreviousConstraints(QVector<id_type<ConstraintModel>>&& vec)
		{ m_previousConstraints = std::move(vec); }

		void setNextConstraints(QVector<id_type<ConstraintModel>>&& vec)
		{ m_nextConstraints = std::move(vec); }

		void setConstraintsYPos(std::unordered_map<id_type<ConstraintModel>,
												   double,
												   id_hash<ConstraintModel>>&& map)
		{ m_constraintsYPos = std::move(map); }

		void setTopY(double val);

		void setBottomY(double val);


		void setOSSIATimeNode(OSSIA::TimeNode* timeEvent)
		{ m_timeEvent = timeEvent; }


		OSSIA::TimeNode* m_timeEvent{};

		id_type<TimeNodeModel> m_timeNode{};

		QVector<id_type<ConstraintModel>> m_previousConstraints;
		QVector<id_type<ConstraintModel>> m_nextConstraints;
		std::unordered_map<id_type<ConstraintModel>, double, id_hash<ConstraintModel>> m_constraintsYPos;

		double m_heightPercentage{0.5};

		double m_topY{0.5};
		double m_bottomY{0.5};

		std::vector<State*> m_states;

		/// TEMPORARY. This information has to be queried from OSSIA::Scenario instead.
		int m_date{0}; // Was : m_x

};
