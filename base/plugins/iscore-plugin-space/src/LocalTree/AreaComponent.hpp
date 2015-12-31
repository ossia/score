#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>
#include <src/SpaceProcess.hpp>
// TODO Cleanme
namespace Space
{
namespace LocalTree
{
class ProcessLocalTree final :
        public Ossia::LocalTree::ProcessComponent
{
        COMPONENT_METADATA(SpaceProcessLocalTree)

         using system_t = Ossia::LocalTree::DocumentPlugin;

     public:
        ProcessLocalTree(
                const Id<Component>& id,
                OSSIA::Node& parent,
                Space::ProcessModel& process,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_obj);

        std::shared_ptr<OSSIA::Node> m_areas;
        std::shared_ptr<OSSIA::Node> m_computations;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

};


class ProcessLocalTreeFactory final :
        public Ossia::LocalTree::ProcessComponentFactory
{
    public:
        virtual ~ProcessLocalTreeFactory();
        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel& p,
                const Ossia::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;

        Ossia::LocalTree::ProcessComponent* make(
                const Id<iscore::Component>& id,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const override;
};


class ISCORE_PLUGIN_SPACE_EXPORT AreaComponent : public iscore::Component
{
    ISCORE_METADATA(Space::LocalTree::AreaComponent)
    public:
        AreaComponent(
                OSSIA::Node& node,
                AreaModel& area,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~AreaComponent();

        const std::shared_ptr<OSSIA::Node>& node() const;

    protected:
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

    private:
        OSSIA::Node& thisNode() const;
        MetadataNamePropertyWrapper m_thisNode;
};

class ISCORE_PLUGIN_SPACE_EXPORT AreaComponentFactory :
        public ::GenericComponentFactory<
        AreaModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::AreaComponent>
{
    public:
        virtual ~AreaComponentFactory();

        virtual AreaComponent* make(
                const Id<iscore::Component>&,
                OSSIA::Node& parent,
                AreaModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const = 0;
};

// TODO return Generic by default
using AreaComponentFactoryList =
    ::GenericComponentFactoryList<
            AreaModel,
            Ossia::LocalTree::DocumentPlugin,
            Space::LocalTree::AreaComponent,
            Space::LocalTree::AreaComponentFactory>;

class GenericAreaComponent final : public AreaComponent
{
        COMPONENT_METADATA(GenericAreaComponent)
    public:
        GenericAreaComponent(
                const Id<iscore::Component>& cmp,
                OSSIA::Node& parent_node,
                AreaModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt);
};

// It must be last in the vector
class GenericAreaComponentFactory final
        : public AreaComponentFactory
{
    private:
        AreaComponent* make(
                        const Id<iscore::Component>& cmp,
                        OSSIA::Node& parent,
                        AreaModel& proc,
                        const Ossia::LocalTree::DocumentPlugin& doc,
                        const iscore::DocumentContext& ctx,
                        QObject* paren_objt) const override;

        const factory_key_type& key_impl() const override;

        bool matches(
                Space::AreaModel& p,
                const Ossia::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;
};
}
}
