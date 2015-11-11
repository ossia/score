#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class SpaceControl : public iscore::PluginControlInterface
{
    public:
        SpaceControl(iscore::Presenter* pres);
        virtual ~SpaceControl();
};
