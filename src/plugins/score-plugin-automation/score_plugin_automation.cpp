// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_automation.hpp"

#include <Automation/AutomationColors.hpp>
#include <Automation/AutomationExecution.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationPresenter.hpp>
#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/AutomationView.hpp>
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Automation/Inspector/AutomationInspectorFactory.hpp>
#include <Automation/Inspector/AutomationStateInspectorFactory.hpp>
#include <Automation/Inspector/CurvePointInspectorFactory.hpp>
#include <Automation/LocalTree.hpp>
#include <Curve/Process/CurveProcessFactory.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <Color/GradientExecution.hpp>
#include <Color/GradientModel.hpp>
#include <Color/GradientPresenter.hpp>
#include <Color/GradientView.hpp>
#include <Metronome/MetronomeColors.hpp>
#include <Metronome/MetronomeExecution.hpp>
#include <Metronome/MetronomeModel.hpp>
#include <Metronome/MetronomePresenter.hpp>
#include <Metronome/MetronomeView.hpp>
#include <Spline/SplineExecution.hpp>
#include <Spline/SplineModel.hpp>
#include <Spline/SplinePresenter.hpp>
#include <Spline/SplineView.hpp>
#include <score_plugin_automation_commands_files.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Automation::LayerPresenter)
W_OBJECT_IMPL(Metronome::LayerPresenter)
namespace Automation
{
template <typename Layer_T>
class MinMaxHeaderDelegate final : public Process::DefaultHeaderDelegate
{
public:
  using model_t = std::remove_reference_t<decltype(std::declval<Layer_T>().model())>;
  MinMaxHeaderDelegate(
      const Process::ProcessModel& m,
      const Process::Context& doc,
      const Layer_T* layer)
      : Process::DefaultHeaderDelegate{m, doc, layer}
  {
    const auto& model = static_cast<model_t&>(m_model);

    con(model, &model_t::minChanged, this, [=] { updateText(); });
    con(model, &model_t::maxChanged, this, [=] { updateText(); });
  }

  void updateText() override
  {
    auto& style = Process::Style::instance();
    const auto& model = static_cast<model_t&>(m_model);

    const QPen& pen = m_sel ? style.IntervalHeaderTextPen() : textPen(style, model);

    QString txt = model.prettyName();
    txt += "  Min: ";
    txt += QString::number(model.min());
    txt += "  Max: ";
    txt += QString::number(model.max());
    m_line = Process::makeGlyphs(txt, pen);
    update();
    updatePorts();
  }
};
using AutomationFactory = Process::ProcessFactory_T<Automation::ProcessModel>;
using AutomationLayerFactory = Curve::CurveLayerFactory_T<
    Automation::ProcessModel,
    Automation::LayerPresenter,
    Automation::LayerView,
    Automation::Colors,
    Automation::MinMaxHeaderDelegate<Automation::LayerPresenter>>;
}
namespace Gradient
{
using GradientFactory = Process::ProcessFactory_T<Gradient::ProcessModel>;
using GradientLayerFactory
    = Process::LayerFactory_T<Gradient::ProcessModel, Gradient::Presenter, Gradient::View>;
}

namespace Spline
{
using SplineFactory = Process::ProcessFactory_T<Spline::ProcessModel>;
using SplineLayerFactory
    = Process::LayerFactory_T<Spline::ProcessModel, Spline::Presenter, Spline::View>;
}

namespace Metronome
{
using MetronomeFactory = Process::ProcessFactory_T<Metronome::ProcessModel>;
using MetronomeLayerFactory = Curve::CurveLayerFactory_T<
    Metronome::ProcessModel,
    Metronome::LayerPresenter,
    Metronome::LayerView,
    Metronome::Colors,
    Automation::MinMaxHeaderDelegate<Metronome::LayerPresenter>>;
}

score_plugin_automation::score_plugin_automation() = default;
score_plugin_automation::~score_plugin_automation() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_automation::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory,
         Automation::AutomationFactory,
         Gradient::GradientFactory,
         Spline::SplineFactory,
         Metronome::MetronomeFactory>,
      FW<Process::LayerFactory,
         Automation::AutomationLayerFactory,
         Gradient::GradientLayerFactory,
         Spline::SplineLayerFactory,
         Metronome::MetronomeLayerFactory>,
      FW<Inspector::InspectorWidgetFactory,
         Automation::StateInspectorFactory,
         Automation::PointInspectorFactory,
         Automation::InspectorFactory,
         Gradient::InspectorFactory,
         Spline::InspectorFactory,
         Metronome::InspectorFactory>,

      FW<LocalTree::ProcessComponentFactory, LocalTree::AutomationComponentFactory>,

      FW<Execution::ProcessComponentFactory,
         //, Interpolation::Executor::ComponentFactory,

         Automation::RecreateOnPlay::ComponentFactory,
         Gradient::RecreateOnPlay::ComponentFactory,
         Spline::RecreateOnPlay::ComponentFactory,
         Metronome::RecreateOnPlay::ComponentFactory>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_automation::make_commands()
{
  using namespace Automation;
  using namespace Gradient;
  using namespace Spline;
  using namespace Metronome;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_automation_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_automation)
