#pragma once
#include <tools/IdentifiedObject.hpp>
#include <QDataStream>

class State : public IdentifiedObject
{
		Q_OBJECT

		friend QDataStream& operator<<(QDataStream& s, const State& state)
		{
			s << state.messages();

			return s;
		}

		friend QDataStream& operator>>(QDataStream& s, State& state)
		{
			QStringList messages;
			s >> messages;
			for(auto& message : messages)
			{
				state.addMessage(message);
			}

			return s;
		}

	public:
		State(int id, QObject* parent):
			IdentifiedObject{id, "State", parent}
		{

		}
		State(QDataStream& s, QObject* parent):
			IdentifiedObject{s, parent}
		{

		}

		virtual ~State() = default;
		virtual QStringList messages() const = 0;
		virtual void addMessage(QString message) = 0;
};

class FakeState : public State
{
		Q_OBJECT

	public:
		using State::State;
		FakeState(QDataStream& s, QObject* parent):
			State{s, parent}
		{
			s >> *this;
		}

		virtual QStringList messages() const override
		{
			return {m_address};
		}

		virtual void addMessage(QString message) override
		{
			m_address = message;
		}

	private:
		QString m_address;
		QVariant val;
};

