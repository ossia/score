#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

namespace OSSIA
{
    class Device;
}
class OSSIAControl : public iscore::PluginControlInterface
{
    public:
        OSSIAControl(iscore::Presenter* pres);

    private:
        std::shared_ptr<OSSIA::Device> m_localDevice;
};
