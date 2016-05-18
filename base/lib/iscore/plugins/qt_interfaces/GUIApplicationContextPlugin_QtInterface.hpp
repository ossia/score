#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>

namespace iscore
{
    struct ApplicationContext;
    class GUIApplicationContextPlugin;

    class ISCORE_LIB_BASE_EXPORT GUIApplicationContextPlugin_QtInterface
    {
        public:
            virtual ~GUIApplicationContextPlugin_QtInterface();

            virtual GUIApplicationContextPlugin* make_applicationPlugin(const iscore::ApplicationContext& app) = 0;
    };
}

#define GUIApplicationContextPlugin_QtInterface_iid "org.ossia.i-score.plugins.GUIApplicationContextPlugin_QtInterface"

Q_DECLARE_INTERFACE(iscore::GUIApplicationContextPlugin_QtInterface, GUIApplicationContextPlugin_QtInterface_iid)
