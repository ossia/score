#pragma once
#include <QObject>
#include <QWidget>
#include <QGraphicsObject>
#include <QDebug>
#include "interface/serialization/VisitorInterface.hpp"

////////////////////////////////////////////////
// This file contains utility algorithms & classes that can be used
// everywhere (core, plugins...)
////////////////////////////////////////////////
template<typename QType>
class NamedType : public QType
{
	public:
		template<typename... Args>
		NamedType(QString name, QObject* parent, Args&&... args):
			QType{std::forward<Args>(args)...}
		{
			QType::setObjectName(name);
			QType::setParent(parent);
		}

		template<typename ReaderImpl,typename... Args>
		NamedType(Deserializer<ReaderImpl>& v, QObject* parent, Args&&... args):
			QType{std::forward<Args>(args)...}
		{
			v.visit(*this);
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
