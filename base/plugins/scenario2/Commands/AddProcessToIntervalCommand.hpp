#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <QNamedObject>
struct ObjectIdentifier
{
		QString child_name;
		int id; // 0 by default;
};
#include <QApplication>
#include <QMetaObject>
class ObjectPath
{
	public:
		QString baseObject;
		std::vector<ObjectIdentifier> v;
		
		QObject* find()
		{
			auto it = v.begin();
			
			QObject* obj = qApp->findChild<QObject*>(baseObject);
			while(it != v.end())
			{
				auto childs = obj->findChildren<QIdentifiedObject*>(it->child_name, Qt::FindDirectChildrenOnly);
				
				auto elt = findById(childs, it->id);
				if(elt) obj = elt;
				else return nullptr;
				
				++it;
			}
			
			return obj;
		}
		
};


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
