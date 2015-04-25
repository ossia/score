#include "ChangeGroup.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventModel.hpp"

GroupMetadata* getGroupMetadata(QObject* obj)
{
    using namespace std;
    if(auto cstr = dynamic_cast<ConstraintModel*>(obj))
    {
        auto& plugs = cstr->pluginModelList().list();
        auto plug_it = std::find_if(begin(plugs), end(plugs), [] (iscore::ElementPluginModel* elt)
        { return elt->metaObject()->className() == QString{"GroupMetadata"}; });
        Q_ASSERT(plug_it != end(plugs));

        return static_cast<GroupMetadata*>(*plug_it);
    }
    else if(auto ev = dynamic_cast<EventModel*>(obj))
    {
        auto& plugs = ev->pluginModelList().list();
        auto plug_it = std::find_if(begin(plugs), end(plugs), [] (iscore::ElementPluginModel* elt)
        { return elt->metaObject()->className() == QString{"GroupMetadata"}; });
        Q_ASSERT(plug_it != end(plugs));

        return static_cast<GroupMetadata*>(*plug_it);
    }

    Q_ASSERT(false);
}

ChangeGroup::ChangeGroup(ObjectPath &&path, id_type<Group> newGroup):
    iscore::SerializableCommand{"NetworkControl", "ChangeGroup", QObject::tr("Change the group of an element")},
    m_path{path},
    m_newGroup{newGroup}
{
    m_oldGroup = getGroupMetadata(m_path.find<QObject>())->group();
}

void ChangeGroup::undo()
{
    getGroupMetadata(m_path.find<QObject>())->setGroup(m_oldGroup);
}

void ChangeGroup::redo()
{
    getGroupMetadata(m_path.find<QObject>())->setGroup(m_newGroup);
}

bool ChangeGroup::mergeWith(const iscore::Command *)
{
    return false;
}

void ChangeGroup::serializeImpl(QDataStream &s) const
{
    s << m_path << m_newGroup << m_oldGroup;
}

void ChangeGroup::deserializeImpl(QDataStream &s)
{
    s >> m_path >> m_newGroup >> m_oldGroup;
}
