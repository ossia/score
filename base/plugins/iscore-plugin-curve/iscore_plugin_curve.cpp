#include "iscore_plugin_curve.hpp"

#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"
#include "Curve/Segment/CurveSegmentList.hpp"

#if defined(ISCORE_LIB_INSPECTOR)
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


#include "Curve/Commands/UpdateCurve.hpp"
#include "Curve/Commands/SetSegmentParameters.hpp"

#include "Commands/ChangeAddress.hpp"
#include "Commands/SetCurveMin.hpp"
#include "Commands/SetCurveMax.hpp"
#include "Commands/InitAutomation.hpp"

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

#if defined(ISCORE_LIB_INSPECTOR)
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


std::pair<const std::string, CommandGeneratorMap> iscore_plugin_curve::make_commands()
{
    std::pair<const std::string, CommandGeneratorMap> cmds;
    boost::mpl::for_each<
            boost::mpl::list<
                UpdateCurve,
                SetSegmentParameters,
                ChangeAddress,
                SetCurveMin,
                SetCurveMax,
                InitAutomation
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter2{cmds.second});

    return cmds;
}
