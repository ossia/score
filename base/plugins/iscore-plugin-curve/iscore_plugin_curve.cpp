#include "iscore_plugin_curve.hpp"

#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"
#include "Curve/Segment/CurveSegmentList.hpp"

#if defined(ISCORE_INSPECTOR_LIB)
#include "Inspector/AutomationInspectorFactory.hpp"
#include "Inspector/AutomationStateInspectorFactory.hpp"
#endif


#include "Curve/Segment/Power/PowerCurveSegmentModel.hpp"
#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
#include "Curve/Segment/Sin/SinCurveSegmentModel.hpp"
#include "Curve/Segment/Gamma/GammaCurveSegmentModel.hpp"

DEFINE_CURVE_FACTORY(LinearCurveSegmentFactory, "Linear", LinearCurveSegmentModel)
DEFINE_CURVE_FACTORY(PowerCurveSegmentFactory, "Power", PowerCurveSegmentModel)
DEFINE_CURVE_FACTORY(SinCurveSegmentFactory, "Sin", SinCurveSegmentModel)
DEFINE_CURVE_FACTORY(GammaCurveSegmentFactory, "Gamma", GammaCurveSegmentModel)


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
