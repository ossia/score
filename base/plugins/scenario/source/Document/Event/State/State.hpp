#pragma once
#include <tools/IdentifiedObject.hpp>
#include <QDataStream>

class State : public IdentifiedObject
{
		Q_OBJECT

	public:
		State(int id, QObject* parent):
			IdentifiedObject{id, "State", parent}
		{
		}

		template<typename Impl>
		State(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObject{vis, parent}
		{
			vis.visit(*this);
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

		template<typename Impl>
		FakeState(Deserializer<Impl>& vis, QObject* parent):
			State{vis, parent}
		{
			// The parent already does everything required
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

