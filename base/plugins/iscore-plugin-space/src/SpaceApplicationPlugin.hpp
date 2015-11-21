#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

class SpaceApplicationPlugin : public iscore::GUIApplicationContextPlugin
{
    public:
        SpaceApplicationPlugin(iscore::Presenter* pres);
        virtual ~SpaceApplicationPlugin();
};
