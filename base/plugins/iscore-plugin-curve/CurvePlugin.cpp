#include "CurvePlugin.hpp"

#include "Inspector/AutomationInspectorFactory.hpp"
#include "Inspector/AutomationStateInspectorFactory.hpp"
#include "Automation/AutomationFactory.hpp"
#include "AutomationControl.hpp"
#include "CurveTest/CurveSegmentList.hpp"

#include "CurveTest/LinearCurveSegmentFactory.hpp"

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
    if(factoryName == "Process")
    {
        return {new AutomationFactory};
    }

    if(factoryName == "Inspector")
    {
        return {new AutomationInspectorFactory,
                new AutomationStateInspectorFactory};
    }

    if(factoryName == "CurveSegment")
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
