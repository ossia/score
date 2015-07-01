#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <memory>

namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}
class OSSIAProcessElement :public iscore::ElementPluginModel
{
    public:
        using iscore::ElementPluginModel::ElementPluginModel;
        virtual std::shared_ptr<OSSIA::TimeProcess> process() const = 0;

};
