// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <iscore/tools/std/HashMap.hpp>

#include "iscore_plugin_curve.hpp"
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Curve/Segment/Noise/NoiseSegment.hpp>
#include <Curve/Segment/Sin/SinSegment.hpp>
#include <Curve/Settings/CurveSettingsFactory.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_plugin_curve_commands_files.hpp>

iscore_plugin_curve::iscore_plugin_curve() : QObject{}
{
  qRegisterMetaType<Curve::Settings::Mode>();
  qRegisterMetaTypeStreamOperators<Curve::Settings::Mode>();
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_curve::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& factoryName) const
{
  return instantiate_factories<iscore::ApplicationContext,
      FW<Curve::SegmentFactory
        , Curve::SegmentFactory_T<Curve::LinearSegment>
        , Curve::SegmentFactory_T<Curve::PowerSegment>
        , Curve::SegmentFactory_T<Curve::NoiseSegment>
        , Curve::SegmentFactory_T<Curve::PointArraySegment>
        , Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Sin>>
        , Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Square>>
        , Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Triangle>>
        , Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Saw>>

      >,
      FW<iscore::SettingsDelegateFactory, Curve::Settings::Factory>>(
      ctx, factoryName);
}

std::vector<std::unique_ptr<iscore::InterfaceListBase>>
iscore_plugin_curve::factoryFamilies()
{
  return make_ptr_vector<iscore::InterfaceListBase, Curve::SegmentList>();
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_curve::make_commands()
{
  using namespace Curve;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Curve::CommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_curve_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}
