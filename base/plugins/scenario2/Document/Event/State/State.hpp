#pragma once
#include <QNamedObject>

class State : public QNamedObject
{
		Q_OBJECT

	public:
		State(QObject* parent): QNamedObject{parent, "State"}
		{

		}

		virtual ~State() = default;
		virtual QStringList messages() const = 0;
		virtual void addMessage(QString message) = 0;

	private:

};

class FakeState : public State
{
		Q_OBJECT

	public:
		using State::State;

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
};

