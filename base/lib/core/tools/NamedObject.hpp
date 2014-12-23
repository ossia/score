#pragma once
#include <QObject>
#include <QWidget>
#include <QGraphicsObject>
#include <QDebug>

////////////////////////////////////////////////
// This file contains utility algorithms & classes that can be used
// everywhere (core, plugins...)
////////////////////////////////////////////////
template<typename QType>
class NamedType : public QType
{
		friend QDataStream& operator << (QDataStream& s, const NamedType<QType>& obj)
		{
			s << obj.objectName();

			return s;
		}

		friend QDataStream& operator >> (QDataStream& s, NamedType<QType>& obj)
		{
			QString objectName;
			s >> objectName;
			obj.setObjectName(objectName);

			return s;
		}

	public:
		template<typename... Args>
		NamedType(QString name, QObject* parent, Args&&... args):
			QType{std::forward<Args>(args)...}
		{
			QType::setObjectName(name);
			QType::setParent(parent);
		}

		template<typename... Args>
		NamedType(QDataStream& s, QObject* parent, Args&&... args):
			QType{std::forward<Args>(args)...}
		{
			s >> *this;
			QType::setParent(parent);
		}
};



using NamedObject = NamedType<QObject>;


inline void debug_parentHierarchy(QObject* obj)
{
	while(obj)
	{
		qDebug() << obj->objectName();
		obj = obj->parent();
	}
}

#define DEMO_PIXEL_SPACING_TEST 5
