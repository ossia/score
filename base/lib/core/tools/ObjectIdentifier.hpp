#pragma once
#include <core/tools/SettableIdentifier.hpp>

/**
 * @brief The ObjectIdentifier class
 *
 * A mean to identify an object without a pointer. The id is useful
 * if the object is inside a collection.
 *
 * Example:
 * @code
 *	ObjectIdentifier ob{"TheObjectName", {}}; // id-less
 *	ObjectIdentifier ob{"TheObjectName", 34}; // with id
 * @endcode
 */
class ObjectIdentifier
{
		friend QDataStream& operator <<(QDataStream& s, const ObjectIdentifier& obj)
		{
			s << obj.m_childName << obj.m_id;
			return s;
		}

		friend QDataStream& operator >>(QDataStream& s, ObjectIdentifier& obj)
		{
			s >> obj.m_childName >> obj.m_id;
			return s;
		}

	public:
		ObjectIdentifier() = default;
		ObjectIdentifier(const char* name):
			m_childName{name}
		{ }

		ObjectIdentifier(const char* name, SettableIdentifier id):
			m_childName{name},
			m_id{std::move(id)}
		{ }

		ObjectIdentifier(QString name, SettableIdentifier id):
			m_childName{std::move(name)},
			m_id{std::move(id)}
		{ }

		const QString& childName() const
		{ return m_childName; }

		const SettableIdentifier& id() const
		{ return m_id; }

	private:
		QString m_childName;
		SettableIdentifier m_id;
};
