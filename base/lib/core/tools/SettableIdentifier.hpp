#pragma once
#include <QDataStream>

class NoIdentifier {};
class SettableIdentifier
{
		friend QDataStream& operator <<(QDataStream& s, const SettableIdentifier& obj)
		{
			s << obj.m_id << obj.m_set;
			return s;
		}

		friend QDataStream& operator >>(QDataStream& s, SettableIdentifier& obj)
		{
			s >> obj.m_id >> obj.m_set;
			return s;
		}

	public:
		using identifier_type = int;

		SettableIdentifier() = default;
		SettableIdentifier(NoIdentifier) { }
		SettableIdentifier(identifier_type i):
			m_id{i},
			m_set{true}
		{ }

		bool set() const
		{ return m_set; }

		operator identifier_type() const
		{ return m_id; }

	private:
		identifier_type m_id{};
		bool m_set{false};
};
