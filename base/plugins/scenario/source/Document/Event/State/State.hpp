#pragma once
#include <tools/NamedObject.hpp>

class State : public NamedObject
{
		Q_OBJECT

	public:
		State(QObject* parent):
			NamedObject{"State", parent}
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
		QVariant val;
};

