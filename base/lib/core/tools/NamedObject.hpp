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
class QNamedType : public QType
{
	public:
		template<typename... Args>
		QNamedType(QType* parent, QString name, Args&&... args):
			QType{std::forward<Args>(args)...}
		{
			QType::setObjectName(name);
			QType::setParent(parent);
		}
};



using QNamedObject = QNamedType<QObject>;
using QNamedGraphicsObject = QNamedType<QGraphicsObject>;
using QNamedWidget = QNamedType<QWidget>;


inline void debug_parentHierarchy(QObject* obj)
{
	while(obj)
	{
		qDebug() << obj->objectName();
		obj = obj->parent();
	}
}

#define DEMO_PIXEL_SPACING_TEST 5
