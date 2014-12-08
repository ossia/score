#pragma once
#include <tools/IdentifiedObject.hpp>

#include <QApplication>
struct ObjectIdentifier
{
		friend QDataStream& operator <<(QDataStream& s, const ObjectIdentifier& obj)
		{
			s << obj.child_name << obj.id;
			return s;
		}

		friend QDataStream& operator >>(QDataStream& s, ObjectIdentifier& obj)
		{
			s >> obj.child_name >> obj.id;
			return s;
		}

		QString child_name;
		int id;
		//bool use_id = true;
};

class ObjectPath
{
	public:
		ObjectPath() = default;
		ObjectPath(const ObjectPath& obj) = default;
		ObjectPath(ObjectPath&&) = default;

		// Creates a path object going from the (named) object with the "origin" name, to the identified object obj.
		static ObjectPath pathFromObject(QString origin, QIdentifiedObject* obj);

		// Find an object in the hierarchy of the application.
		QObject* find();


		QString baseObject;
		std::vector<ObjectIdentifier> v;

};

QDataStream& operator <<(QDataStream& s, const ObjectPath& path);
QDataStream& operator >>(QDataStream& s, ObjectPath& path);
