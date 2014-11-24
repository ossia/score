#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
/*
class ObjectIdentifier
{
	public:
		int id{}; // 0 by default;
		QString child_name;
};
#include <QApplication>
#include <QMetaObject>
class ObjectPath
{
	public:
		QString baseObject;
		char* x = "BaseInterval, \
				  3 : Scenario, \
				  2 : Interval, \
				  3 : Scenario, \
				  1 : Interval";
		
					  
		QObject* find()
		{
			auto it = v.begin();
			
			QObject* obj = qApp->findChild<QObject*>(baseObject);
			while(it != v.end())
			{
				auto childs = obj->findChildren<QObject*>(it->child_name);
				for(auto& elt : childs)
				{
					QMetaObject* metaobj = elt->metaObject();
					QGenericReturnArgument ret;
					auto meth_id = metaobj->invokeMethod(elt, 
														 "id",
														 Qt::DirectConnection,
														 ret);
					ret.data();
				}
			}
		}
		std::vector<ObjectIdentifier> v;
};*/


class AddProcessToIntervalCommand : public iscore::SerializableCommand
{
	public:
//		AddProcessToIntervalCommand(int intervalId, );
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;
};
