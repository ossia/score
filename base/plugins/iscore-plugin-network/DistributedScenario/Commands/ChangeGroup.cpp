#include "ChangeGroup.hpp"
#include "Scenario/Document/Constraint/ConstraintModel.hpp"
#include "Scenario/Document/Event/EventModel.hpp"
#include <boost/range/algorithm/find_if.hpp>
GroupMetadata* getGroupMetadata(QObject* obj)
{
    using namespace boost::range;
    if(auto cstr = dynamic_cast<ConstraintModel*>(obj))
    {
        auto& plugs = cstr->pluginModelList.list();
        auto plug_it = find_if(plugs, [] (iscore::ElementPluginModel* elt)
        { return elt->metaObject()->className() == QString{"GroupMetadata"}; });
        ISCORE_ASSERT(plug_it != plugs.end());

        return static_cast<GroupMetadata*>(*plug_it);
    }
    else if(auto ev = dynamic_cast<EventModel*>(obj))
    {
        auto& plugs = ev->pluginModelList.list();
        auto plug_it = find_if(plugs, [] (iscore::ElementPluginModel* elt)
        { return elt->metaObject()->className() == QString{"GroupMetadata"}; });
        ISCORE_ASSERT(plug_it != plugs.end());

        return static_cast<GroupMetadata*>(*plug_it);
    }

    ISCORE_ABORT;
    return nullptr;
}

ChangeGroup::ChangeGroup(ObjectPath &&path, Id<Group> newGroup):
    m_path{path},
    m_newGroup{newGroup}
{
    m_oldGroup = getGroupMetadata(&m_path.find<QObject>())->group();
}

void ChangeGroup::undo() const
{
    getGroupMetadata(&m_path.find<QObject>())->setGroup(m_oldGroup);
}

void ChangeGroup::redo() const
{
    getGroupMetadata(&m_path.find<QObject>())->setGroup(m_newGroup);
}

void ChangeGroup::serializeImpl(DataStreamInput &s) const
{
    s << m_path << m_newGroup << m_oldGroup;
}

void ChangeGroup::deserializeImpl(DataStreamOutput &s)
{
    s >> m_path >> m_newGroup >> m_oldGroup;
}
