#include "iscore_plugin_curve.hpp"

#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"
#include "Curve/Segment/CurveSegmentList.hpp"

#include "Curve/Segment/LinearCurveSegmentFactory.hpp"
#include "Curve/Segment/GammaCurveSegmentFactory.hpp"
#include "Curve/Segment/Power/PowerCurveSegmentFactory.hpp"
#include "Curve/Segment/SinCurveSegmentFactory.hpp"

#if defined(ISCORE_INSPECTOR_LIB)
#include "Inspector/AutomationInspectorFactory.hpp"
#include "Inspector/AutomationStateInspectorFactory.hpp"
#endif

iscore_plugin_curve::iscore_plugin_curve() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_curve::make_control(iscore::Presenter* pres)
{
    return new AutomationControl{pres};
}

QVector<iscore::FactoryInterface*> iscore_plugin_curve::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new AutomationFactory};
    }

#if defined(ISCORE_INSPECTOR_LIB)
    if(factoryName == InspectorWidgetFactory::factoryName())
    {
        return {new AutomationInspectorFactory,
                new AutomationStateInspectorFactory};
    }
#endif

    if(factoryName == CurveSegmentFactory::factoryName())
    {
        return {new LinearCurveSegmentFactory,
                new PowerCurveSegmentFactory/*,
                new GammaCurveSegmentFactory,
                new SinCurveSegmentFactory*/};
    }

    return {};
}

QVector<iscore::FactoryFamily> iscore_plugin_curve::factoryFamilies()
{
    return {{CurveSegmentFactory::factoryName(),
             [&] (iscore::FactoryInterface* fact)
             { SingletonCurveSegmentList::instance().registerFactory(safe_cast<CurveSegmentFactory*>(fact)); }
           }};
}
