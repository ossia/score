#pragma once
#include <QObject>

namespace iscore
{
class Presenter;
    class PluginControlInterface;
    class PluginControlInterface_QtInterface
    {
        public:
            virtual ~PluginControlInterface_QtInterface() = default;

            virtual PluginControlInterface* make_control(Presenter* parent) = 0;
    };
}


#define PluginControlInterface_QtInterface_iid "org.ossia.i-score.plugins.PluginControlInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PluginControlInterface_QtInterface, PluginControlInterface_QtInterface_iid)
