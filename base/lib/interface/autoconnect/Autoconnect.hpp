#pragma once
#include <QObject>
#include <QGraphicsObject>
#include <QDebug>
namespace iscore
{
    /**
     * @brief The Autoconnect class
     *
     * Allows for auto-connection of two objects, either by name or by inheritance relationship.
     * Is done at runtime ( http://www.qtforum.org/article/515/connecting-signals-and-slots-by-name-at-runtime.html ), by Application::doConnections
     */
    class Autoconnect
    {
        public:
            enum class ObjectRepresentationType
            {
                QObjectName, Inheritance
            };
            struct HalfOfConnection
            {
                HalfOfConnection (const HalfOfConnection&) = default;
                HalfOfConnection& operator= (const HalfOfConnection&) = default;
                ObjectRepresentationType type;
                const char* name;
                const char* method;
            };

            Autoconnect (const Autoconnect&) = default;
            Autoconnect& operator= (const Autoconnect&) = default;

            HalfOfConnection source;
            HalfOfConnection target;

            QList<QObject*> getMatchingChildren (const HalfOfConnection& obj_repr,
                                                 const QObject* obj) const
            {
                QList<QObject*> children;

                switch (obj_repr.type)
                {
                    case ObjectRepresentationType::QObjectName:
                    {
                        children = obj->findChildren<QObject*> (obj_repr.name);
                        break;
                    }

                    case ObjectRepresentationType::Inheritance:
                    {
                        for (auto& elt : obj->findChildren<QObject*>() )
                        {
                            if (elt->inherits (obj_repr.name) )
                            {
                                children.append (elt);
                            }
                        }

                        break;
                    }
                }

                return children;
            }

            // Cannot call without arguments.
            QList<QObject*> getMatchingChildrenForSource() const = delete;
            QList<QObject*> getMatchingChildrenForTarget() const = delete;

            template<typename Arg>
            QList<QObject*> getMatchingChildrenForSource (Arg&& obj) const
            {
                return getMatchingChildren (source, obj);
            }

            template<typename Arg, typename... ObjectType>
            QList<QObject*> getMatchingChildrenForSource (Arg obj, ObjectType&& ... further_objs) const
            {
                return getMatchingChildrenForSource (obj) +
                       getMatchingChildrenForSource (std::forward<ObjectType> (further_objs)...);
            }

            template<typename Arg>
            QList<QObject*> getMatchingChildrenForTarget (Arg&& obj) const
            {
                return getMatchingChildren (target, obj);
            }

            template<typename Arg, typename... ObjectType>
            QList<QObject*> getMatchingChildrenForTarget (Arg obj, ObjectType&& ... further_objs) const
            {
                return getMatchingChildrenForTarget (obj) +
                       getMatchingChildrenForTarget (std::forward<ObjectType> (further_objs)...);
            }


    };
}
