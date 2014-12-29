#pragma once
#include <core/tools/ObjectIdentifier.hpp>
#include <QVector>

/**
 * @brief The ObjectPath class
 *
 * A path in the QObject hierarchy, using names and identifiers.
 * See @c{NamedObject} and @c{IdentifiedObject}.
 *
 * If the object id is not known at compile time a path may be created using ObjectPath::pathFromObject.
 *
 * Example:
 * @code
 *	ObjectPath p{ {"MyObject", {}}, {"SomeSubObjectInACollection", 27} };
 * @endcode
*/
class ObjectPath
{
		friend Serializer<DataStream>;
		friend Serializer<JSON>;
		friend Deserializer<DataStream>;
		friend Deserializer<JSON>;

		friend bool operator==(const ObjectPath& lhs, const ObjectPath& rhs)
		{
			return lhs.m_objectIdentifiers == rhs.m_objectIdentifiers;
		}

	public:
		ObjectPath() = default;
		QString toString() const;

		ObjectPath(QVector<ObjectIdentifier>&& vec):
			m_objectIdentifiers{std::move(vec)}
		{
		}

		ObjectPath(std::initializer_list<ObjectIdentifier> lst):
			m_objectIdentifiers{lst}
		{
		}

		ObjectPath(const ObjectPath& obj) = default;
		ObjectPath(ObjectPath&&) = default;
		ObjectPath& operator=(ObjectPath&&) = default;

		/**
		 * @brief pathFromObject Factory method for ObjectPath
		 * @param origin Name of the object from which the search is to be started.
		 * @param obj Pointer of the object to find.
		 *
		 * @return An object path allowing to find again "obj" in the software object hierarchy
		 * (that can be serialized to text, and does not use pointers).
		 */
		static ObjectPath pathFromObject(QString origin, QObject* obj);

		static ObjectPath pathBetweenObjects(const QObject* const parent_obj, QObject* target_object);
		static ObjectPath pathFromObject(QObject* origin_object);

		/**
		 * @brief find the object described by the ObjectPath
		 * @return A pointer to the object, or nullptr if it is not found
		 *
		 * This search starts from qApp.
		 * @todo (maybe) a way to specify custom ways of finding an object
		 * (for instance if obj->blurb() == Ding::someDing)
		 * @todo search starting from another object, for more performance.
		 */
		template<typename T>
		T* find() const
		{
			return static_cast<T*>(find_impl());
		}

	private:
		QObject* find_impl() const;
		QVector<ObjectIdentifier> m_objectIdentifiers;
};

