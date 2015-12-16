#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

class SpaceApplicationPlugin : public iscore::GUIApplicationContextPlugin
{
    public:
        SpaceApplicationPlugin(const iscore::ApplicationContext& pres);
        virtual ~SpaceApplicationPlugin();
};
