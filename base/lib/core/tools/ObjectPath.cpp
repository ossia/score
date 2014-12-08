#include <tools/ObjectPath.hpp>


ObjectPath ObjectPath::pathFromObject(QString origin, QIdentifiedObject* obj)
{
	std::vector<ObjectIdentifier> v;
	QObject* obj_origin = qApp->findChild<QObject*>(origin);

	auto parent = obj;
	while(parent != obj_origin)
	{
		v.push_back({parent->objectName(), parent->id()});

		auto tmp_parent = parent;
		parent = dynamic_cast<QIdentifiedObject*>(tmp_parent->parent());
		if(!parent)
		{
			auto parent2 = dynamic_cast<QObject*>(tmp_parent->parent());
			if(parent2 && parent2->objectName() == origin)
			{
				break;
			}

			throw std::runtime_error("Could not find parent object");
		}
	}

	std::reverse(std::begin(v), std::end(v));

	return {origin, v};
}

QObject*ObjectPath::find()
{
	QObject* obj = qApp->findChild<QObject*>(baseObject);

	for(auto it = v.begin(); it != v.end(); ++it)
	{
		if(it->id != -1) // TODO : instead use a Value class that can be Uninitialized
		{
			auto childs = obj->findChildren<QIdentifiedObject*>(it->child_name, Qt::FindDirectChildrenOnly);

			auto elt = findById(childs, it->id);
			if(!elt)
			{
				return nullptr;
			}

			obj = elt;
		}
		else
		{
			auto child = obj->findChild<NamedObject*>(it->child_name, Qt::FindDirectChildrenOnly);
			if(!child)
			{
				return nullptr;
			}

			obj = child;
		}
	}

	return obj;
}
