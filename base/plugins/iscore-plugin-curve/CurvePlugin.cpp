#include "CurvePlugin.hpp"

#include "Inspector/AutomationInspectorFactory.hpp"
#include "Inspector/AutomationStateInspectorFactory.hpp"
#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"
#include "Curve/Segment/CurveSegmentList.hpp"

#include "Curve/Segment/LinearCurveSegmentFactory.hpp"
#include "Curve/Segment/GammaCurveSegmentFactory.hpp"
#include "Curve/Segment/SinCurveSegmentFactory.hpp"

iscore_plugin_curve::iscore_plugin_curve() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_curve::make_control(iscore::Presenter* pres)
{
    delete m_control;
    m_control = new AutomationControl{pres};
    return m_control;
}

QVector<iscore::FactoryInterface*> iscore_plugin_curve::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new AutomationFactory};
    }

    if(factoryName == InspectorWidgetFactory::factoryName())
    {
        return {new AutomationInspectorFactory,
                new AutomationStateInspectorFactory};
    }

    if(factoryName == CurveSegmentFactory::factoryName())
    {
        return {new LinearCurveSegmentFactory,
                new GammaCurveSegmentFactory,
                new SinCurveSegmentFactory};
    }

    return {};
}

QVector<iscore::FactoryFamily> iscore_plugin_curve::factoryFamilies()
{
    return {{"CurveSegment",
             [&] (iscore::FactoryInterface* fact)
             { SingletonCurveSegmentList::instance().registerFactory(static_cast<CurveSegmentFactory*>(fact)); }
           }};
}
