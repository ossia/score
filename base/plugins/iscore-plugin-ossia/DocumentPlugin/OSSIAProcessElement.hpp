#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <memory>

namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}
class OSSIAProcessElement :public QObject
{
    public:
        using QObject::QObject;
        virtual std::shared_ptr<OSSIA::TimeProcess> process() const = 0;
        virtual void stop(){}

};
