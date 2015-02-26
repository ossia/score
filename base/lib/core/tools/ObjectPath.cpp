#include <QApplication>
#include <core/tools/IdentifiedObject.hpp>
#include <core/tools/ObjectPath.hpp>

ObjectPath ObjectPath::pathBetweenObjects(const QObject* const parent_obj, const QObject* target_object)
{
    QVector<ObjectIdentifier> v;

    auto current_obj = target_object;
    auto add_parent_to_vector = [&v](const QObject * ptr)
    {
        if(auto id_obj = dynamic_cast<const IdentifiedObjectAbstract*>(ptr))
            v.push_back({id_obj->objectName(), id_obj->id_val() });
        else
            v.push_back({ptr->objectName(), {}});
    };

    // Recursively go through the object and all the parents
    while(current_obj != parent_obj)
    {
        if(current_obj->objectName().isEmpty())
        {
            throw std::runtime_error("ObjectPath::pathFromObject : an object in the hierarchy does not have a name.");
        }

        add_parent_to_vector(current_obj);

        current_obj = current_obj->parent();

        if(!current_obj)
        {
            throw std::runtime_error("ObjectPath::pathFromObject : Could not find path to parent object");
        }
    }

    // Add the last parent (the one specified with parent_name)
    add_parent_to_vector(current_obj);

    // Search goes from top to bottom (of the parent hierarchy) instead
    std::reverse(std::begin(v), std::end(v));
    return std::move(v);
}

QString ObjectPath::toString() const
{
    QString s;

    for(auto& obj : m_objectIdentifiers)
    {
        s += obj.objectName();

        if(obj.id())
        {
            s += ".";
            s += QString::number(*obj.id());
        }

        s += "/";
    }

    return s;
}

ObjectPath ObjectPath::pathFromObject(QString parent_name, QObject* target_object)
{
    QObject* parent_obj = qApp->findChild<QObject*> (parent_name);
    return ObjectPath::pathBetweenObjects(parent_obj, target_object);
}

ObjectPath ObjectPath::pathFromObject(QObject* origin_object)
{
    auto path = ObjectPath::pathBetweenObjects(qApp, origin_object);
    path.m_objectIdentifiers.removeFirst();
    return path;
}

QObject* ObjectPath::find_impl() const
{
    auto parent_name = m_objectIdentifiers.at(0).objectName();
    std::vector<ObjectIdentifier> children(m_objectIdentifiers.size() - 1);
    std::copy(std::begin(m_objectIdentifiers) + 1,
              std::end(m_objectIdentifiers),
              std::begin(children));

    QObject* obj = qApp->findChild<QObject*> (parent_name);

    for(const auto& currentObjIdentifier : children)
    {
        if(currentObjIdentifier.id())
        {
            auto children = obj->findChildren<IdentifiedObjectAbstract*> (currentObjIdentifier.objectName(),
                            Qt::FindDirectChildrenOnly);

            obj = findById(children,
                           *currentObjIdentifier.id());
        }
        else
        {
            auto child = obj->findChild<NamedObject*> (currentObjIdentifier.objectName(),
                         Qt::FindDirectChildrenOnly);

            if(!child)
            {
                throw std::runtime_error("ObjectPath::find  Error! Child not found");
            }

            obj = child;
        }
    }

    return obj;
}
