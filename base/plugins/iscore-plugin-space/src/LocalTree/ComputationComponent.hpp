#pragma once
#include <OSSIA/LocalTree/NameProperty.hpp>
#include <src/Computation/ComputationModel.hpp>

#include <iscore/tools/Metadata.hpp>
#include <iscore_plugin_space_export.h>

namespace Space
{
namespace LocalTree
{
class ISCORE_PLUGIN_SPACE_EXPORT ComputationComponent : public iscore::Component
{
    ISCORE_METADATA(Space::LocalTree::ComputationComponent)
    public:
        ComputationComponent(
                OSSIA::Node& node,
                ComputationModel& Computation,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~ComputationComponent();

        const std::shared_ptr<OSSIA::Node>& node() const;
        const std::shared_ptr<OSSIA::Node>& valueNode() const
        { return m_valueNode; }

    protected:
        OSSIA::Node& thisNode() const;

    private:
        MetadataNamePropertyWrapper m_thisNode;

    protected:
        std::shared_ptr<OSSIA::Node> m_valueNode;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

}
}
