#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>

#include <memory>
class TimeNodeModel;
namespace OSSIA
{
    class TimeNode;
}

class OSSIATimeNodeElement : public iscore::ElementPluginModel
{
    public:
        static constexpr iscore::ElementPluginModelType staticPluginId() { return 3; }
        OSSIATimeNodeElement(
                std::shared_ptr<OSSIA::TimeNode> ossia_tn,
                const TimeNodeModel& element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeNode> timeNode() const;

        iscore::ElementPluginModelType elementPluginId() const;

        iscore::ElementPluginModel*clone(
                const QObject* element,
                QObject* parent) const override;

        void serialize(const VisitorVariant&) const;

    private:
        std::shared_ptr<OSSIA::TimeNode> m_node;
};
