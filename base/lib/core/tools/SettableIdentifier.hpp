#pragma once
#include <QDataStream>
#include "interface/serialization/DataStreamVisitor.hpp"
#include "interface/serialization/JSONVisitor.hpp"

class NoIdentifier {};
class SettableIdentifier
{
		friend Serializer<DataStream>;
		friend Serializer<JSON>;
		friend Deserializer<DataStream>;
		friend Deserializer<JSON>;

	public:
		using identifier_type = int;

		friend bool operator==(const SettableIdentifier& lhs, const SettableIdentifier& rhs)
		{
			return (lhs.m_set && rhs.m_set && (lhs.m_id == rhs.m_id)) || (!lhs.m_set && !rhs.m_set);
		}
		friend bool operator==(const SettableIdentifier& lhs, identifier_type& id)
		{
			return (lhs.m_set && (lhs.m_id == id));
		}


		SettableIdentifier() = default;
		SettableIdentifier(NoIdentifier) { }
		SettableIdentifier(identifier_type i):
			m_id{i},
			m_set{true}
		{ }

		bool set() const
		{ return m_set; }

		explicit operator identifier_type() const
		{ return m_id; }

	private:
		identifier_type m_id{};
		bool m_set{false};
};

Q_DECLARE_METATYPE(SettableIdentifier)
