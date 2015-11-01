#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class CurveSegmentList;
class MappingControl : public iscore::PluginControlInterface
{
    public:
        explicit MappingControl(iscore::Presenter* pres);
        virtual ~MappingControl() = default;

    private:
        void initColors();
};
