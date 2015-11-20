#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

class SpaceControl : public iscore::GUIApplicationContextPlugin
{
    public:
        SpaceControl(iscore::Presenter* pres);
        virtual ~SpaceControl();
};
