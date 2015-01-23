#pragma once
#include <core/tools/SettableIdentifier.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>
#include <interface/serialization/JSONVisitor.hpp>


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
		friend Serializer<DataStream>;
		friend Serializer<JSON>;
		friend Deserializer<DataStream>;
		friend Deserializer<JSON>;

		friend bool operator==(const ObjectIdentifier& lhs, const ObjectIdentifier& rhs)
		{
			return (lhs.m_objectName == rhs.m_objectName) && (lhs.m_id == rhs.m_id);
		}
	public:
		ObjectIdentifier() = default;
		ObjectIdentifier(const char* name):
			m_objectName{name}
		{ }

		ObjectIdentifier(QString name, boost::optional<int32_t> id):
			m_objectName{std::move(name)},
			m_id{std::move(id)}
		{ }

		template<typename T>
		ObjectIdentifier(QString name, id_type<T> id):
			m_objectName{std::move(name)}
		{
			if(id.val().is_initialized())
				m_id = id.val().get();
		}

		const QString& objectName() const
		{ return m_objectName; }

		const boost::optional<int32_t>& id() const
		{ return m_id; }

	private:
		QString m_objectName;
		boost::optional<int32_t> m_id;
};

Q_DECLARE_METATYPE(ObjectIdentifier)

typedef QVector<ObjectIdentifier> ObjectIdentifierVector;

