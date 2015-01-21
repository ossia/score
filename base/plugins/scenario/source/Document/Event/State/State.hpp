#pragma once
#include <tools/SettableIdentifierAlternative.hpp>

class State : public IdentifiedObjectAlternative<State>
{
		Q_OBJECT

	public:
		State(id_type<State> id, QObject* parent):
			IdentifiedObjectAlternative<State>{id, "State", parent}
		{
		}

		template<typename Impl>
		State(Deserializer<Impl>& vis, QObject* parent):
			IdentifiedObjectAlternative<State>{vis, parent}
		{
			vis.writeTo(*this);
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

