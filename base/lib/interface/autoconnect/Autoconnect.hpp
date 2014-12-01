#pragma once
#include <QObject>
#include <QGraphicsObject>
#include <QDebug>
namespace iscore
{
	/*
	 * auto-connect : faire à chaque fois qu'on fait un make_qqch sur un truc issu d'un plug-in (ou qu'on reçoit un childEvent?).
	 * Pour que ça marche on utilise la notion parent - children des QObject, qui permettent
	 * facilement de faire une recherche récursive.
	 */
	// CF : http://www.qtforum.org/article/515/connecting-signals-and-slots-by-name-at-runtime.html
	
	/**
	 * @brief The Autoconnect class
	 *
	 * Allows for auto-connection of two objects, either by name or by inheritance relationship.
	 */
	class Autoconnect
	{
		public:
			enum class ObjectRepresentationType { QObjectName, Inheritance };
			struct ObjectRepresentation
			{
					ObjectRepresentation(const ObjectRepresentation&) = default;
					ObjectRepresentation& operator=(const ObjectRepresentation&) = default;
					ObjectRepresentationType type;
					const char* name;
					const char* method;
			};

			Autoconnect(const Autoconnect&) = default;
			Autoconnect& operator=(const Autoconnect&) = default;

			ObjectRepresentation source;
			ObjectRepresentation target;

			QList<QObject*> getMatchingChildren(const ObjectRepresentation& obj_repr,
												const QObject* obj) const
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
						for(auto& elt : obj->findChildren<QObject*>())
						{
							if(elt->inherits(obj_repr.name))
							{
								children.append(elt);
							}
						}
						break;
					}
				}

				return children;
			}

			QList<QObject*> getMatchingChildrenForSource() const
			{
				return QList<QObject*>{};
			}
			
			// TODO put variadic templates in here to have infinite appending on the QList.
			template<typename Arg>  // static assert that Arg has a pointer somewhere ?
			QList<QObject*> getMatchingChildrenForSource(Arg&& obj) const
			{
				return getMatchingChildren(source, obj);
			}
			
			template<typename Arg, typename... ObjectType>
			QList<QObject*> getMatchingChildrenForSource(Arg obj, ObjectType&&... further_objs) const
			{
				return getMatchingChildrenForSource(obj) + 
					   getMatchingChildrenForSource(std::forward<ObjectType>(further_objs)...);
			}
			
			template<typename Arg>  // static assert that Arg has a pointer somewhere ?
			QList<QObject*> getMatchingChildrenForTarget(Arg&& obj) const
			{
				return getMatchingChildren(target, obj);
			}
			
			template<typename Arg, typename... ObjectType>
			QList<QObject*> getMatchingChildrenForTarget(Arg obj, ObjectType&&... further_objs) const
			{
				return getMatchingChildrenForTarget(obj) + 
					   getMatchingChildrenForTarget(std::forward<ObjectType>(further_objs)...);
			}
			
			QList<QObject*> getMatchingChildrenForTarget() const
			{
				return QList<QObject*>{};
			}
			
			
			/// OK
/*
			QList<QObject*> getMatchingChildrenForSource(const QObject* obj) const
			{
				return std::move(getMatchingChildren(source, obj));
			}
*/
			/*
			QList<QObject*> getMatchingChildrenForTarget(const QObject* obj) const
			{
				return std::move(getMatchingChildren(target, obj));
			}*/
	};
}
