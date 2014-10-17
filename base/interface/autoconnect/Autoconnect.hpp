#pragma once
#include <QObject>
namespace iscore
{
	/*
	 * auto-connect : faire à chaque fois qu'on fait un make_qqch sur un truc issu d'un plug-in (ou qu'on reçoit un childEvent?).
	 * Pour que ça marche on utilise la notion parent - children des QObject, qui permettent
	 * facilement de faire une recherche récursive.
	 */
	// CF : http://www.qtforum.org/article/515/connecting-signals-and-slots-by-name-at-runtime.html
	class Autoconnect
	{
		public:
			enum class ObjectRepresentationType { QObjectName, Inheritance };
			struct ObjectRepresentation 
			{
					const ObjectRepresentationType type;
					const char* name;
					const char* method;
			};

			const ObjectRepresentation source;
			const ObjectRepresentation target;
			
			QList<QObject*> getMatchingChildren(const ObjectRepresentation& obj_repr,  const QObject* obj) const
			{
				QList<QObject*> children;
				switch(obj_repr.type)
				{
					case ObjectRepresentationType::QObjectName:
					{
						children = obj->findChildren<QObject*>(obj_repr.name);
						break;
					}
					case ObjectRepresentationType::Inheritance:
					{
						auto l1 = obj->findChildren<QObject*>();
						for(auto& elt : l1)
						{
							if(elt->inherits(obj_repr.name))
							{
								children.push_back(elt);
							}
						}
						break;
					}
				}
				
				return children;
			}
			
			QList<QObject*> getMatchingChildrenForSource(const QObject* obj) const
			{
				return std::move(getMatchingChildren(source, obj));
			}
			QList<QObject*> getMatchingChildrenForTarget(const QObject* obj) const
			{
				return std::move(getMatchingChildren(target, obj));
			}
	};
}
