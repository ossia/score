#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>

#include <memory>
class TimeNodeModel;
namespace OSSIA
{
    class TimeNode;
}

class OSSIATimeNodeElement : public QObject
{
    public:
        OSSIATimeNodeElement(
                std::shared_ptr<OSSIA::TimeNode> ossia_tn,
                const TimeNodeModel& element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeNode> timeNode() const;

    private:
        std::shared_ptr<OSSIA::TimeNode> m_node;
};
