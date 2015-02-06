#pragma once
#include <tools/IdentifiedObject.hpp>

class State : public IdentifiedObject<State>
{
		Q_OBJECT

	public:
		State(id_type<State> id, QObject* parent):
			IdentifiedObject<State>{id, "State", parent}
		{
		}

		template<typename Impl>
		State(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObject<State>{vis, parent}
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

		template<typename Impl>
		FakeState(Deserializer<Impl>& vis, QObject* parent):
			State{vis, parent}
		{
			// Else the vtable is not entirely populated
			vis.writeTo(static_cast<State&>(*this));
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

