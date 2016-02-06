#include <unordered_map>

#include <Curve/Commands/CurveCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/Linear/LinearCurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>
#include <Curve/Segment/Sin/SinCurveSegmentModel.hpp>
#include <Curve/Segment/Gamma/GammaCurveSegmentModel.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_curve.hpp"
#include <iscore_plugin_curve_commands_files.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

namespace iscore {
class FactoryListInterface;
}  // namespace iscore

namespace Curve
{
DEFINE_CURVE_SEGMENT_FACTORY(LinearCurveSegmentFactory, Curve::LinearSegment)
DEFINE_CURVE_SEGMENT_FACTORY(PowerCurveSegmentFactory, Curve::PowerSegment)
DEFINE_CURVE_SEGMENT_FACTORY(SinCurveSegmentFactory, Curve::SinSegment)
DEFINE_CURVE_SEGMENT_FACTORY(GammaCurveSegmentFactory, Curve::GammaSegment)
}
iscore_plugin_curve::iscore_plugin_curve() :
    QObject {}
{
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_curve::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& factoryName) const
{
    return instantiate_factories<
       iscore::ApplicationContext,
       TL<
         FW<Curve::SegmentFactory,
             Curve::LinearCurveSegmentFactory,
             Curve::PowerCurveSegmentFactory>>>(ctx, factoryName);
}



std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_curve::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Curve::SegmentList>();
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_curve::make_commands()
{
    using namespace Curve;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{
        Curve::CommandFactoryName(),
                CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_curve_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}

iscore::Version iscore_plugin_curve::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_curve::key() const
{
    return "49837ed7-dbc5-4330-9890-a130a2718b5e";
}
