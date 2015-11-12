#include "iscore_plugin_curve.hpp"

#include "Curve/Segment/CurveSegmentList.hpp"

#include "Curve/Segment/Power/PowerCurveSegmentModel.hpp"
#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
#include "Curve/Segment/Sin/SinCurveSegmentModel.hpp"
#include "Curve/Segment/Gamma/GammaCurveSegmentModel.hpp"

DEFINE_CURVE_SEGMENT_FACTORY(LinearCurveSegmentFactory, "Linear", LinearCurveSegmentModel)
DEFINE_CURVE_SEGMENT_FACTORY(PowerCurveSegmentFactory, "Power", PowerCurveSegmentModel)
DEFINE_CURVE_SEGMENT_FACTORY(SinCurveSegmentFactory, "Sin", SinCurveSegmentModel)
DEFINE_CURVE_SEGMENT_FACTORY(GammaCurveSegmentFactory, "Gamma", GammaCurveSegmentModel)


#include "Curve/Commands/UpdateCurve.hpp"
#include "Curve/Commands/SetSegmentParameters.hpp"

iscore_plugin_curve::iscore_plugin_curve() :
    QObject {}
{
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_curve::factories(const std::string& factoryName) const
{
    if(factoryName == CurveSegmentFactory::staticFactoryName())
    {
        // TODO make matching factories for OSSIA.
        return {new LinearCurveSegmentFactory,
                new PowerCurveSegmentFactory/*,
                new GammaCurveSegmentFactory,
                new SinCurveSegmentFactory*/};
    }

    return {};
}

std::vector<iscore::FactoryFamily> iscore_plugin_curve::factoryFamilies()
{
    return {{CurveSegmentFactory::staticFactoryName(),
             [&] (iscore::FactoryInterfaceBase* f)
             {
                if(auto curve = dynamic_cast<CurveSegmentFactory*>(f))
                    SingletonCurveSegmentList::instance().inscribe(curve);
            }
           }};
}

std::pair<const std::string, CommandGeneratorMap> iscore_plugin_curve::make_commands()
{
    std::pair<const std::string, CommandGeneratorMap> cmds{CurveCommandFactoryName(), CommandGeneratorMap{}};
    boost::mpl::for_each<
            boost::mpl::list<
                UpdateCurve,
                SetSegmentParameters
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
