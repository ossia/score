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
			s << obj.m_objectName << obj.m_id;
			return s;
		}

		friend QDataStream& operator >>(QDataStream& s, ObjectIdentifier& obj)
		{
			s >> obj.m_objectName >> obj.m_id;
			return s;
		}

	public:
		ObjectIdentifier() = default;
		ObjectIdentifier(const char* name):
			m_objectName{name}
		{ }

		ObjectIdentifier(const char* name, SettableIdentifier id):
			m_objectName{name},
			m_id{std::move(id)}
		{ }

		ObjectIdentifier(QString name, SettableIdentifier id):
			m_objectName{std::move(name)},
			m_id{std::move(id)}
		{ }

		const QString& objectName() const
		{ return m_objectName; }

		const SettableIdentifier& id() const
		{ return m_id; }

	private:
		QString m_objectName;
		SettableIdentifier m_id;
};
