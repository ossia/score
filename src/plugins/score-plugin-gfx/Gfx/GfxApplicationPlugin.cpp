#include "GfxApplicationPlugin.hpp"

#include <Process/PreviewSettings.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/Profiler/Profiler.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QAction>
#include <QTimerEvent>

#include <unordered_map>
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif
#include <QToolBar>

namespace Gfx
{

DocumentPlugin::DocumentPlugin(const score::DocumentContext& ctx, QObject* parent)
    : score::DocumentPlugin{ctx, "Gfx::DocumentPlugin", parent}
    , context{ctx}
{
  auto& exec_plug = ctx.plugin<Execution::DocumentPlugin>();
  exec_plug.registerAction(exec);

  con(exec_plug.settings, &Execution::Settings::Model::BenchChanged, this,
      [this](bool) { updateProfiling(); });
  exec_plug.profiler().addSource(this, [this] { pullProfile(); });
  updateProfiling();
}

DocumentPlugin::~DocumentPlugin() { }

void DocumentPlugin::updateProfiling()
{
  auto& exec_plug = score::DocumentPlugin::context().plugin<Execution::DocumentPlugin>();
  const bool on = exec_plug.settings.getBench();
  context.profiler().setEnabled(on);
  if(on && m_profileTimer < 0)
    m_profileTimer = startTimer(200);
  else if(!on && m_profileTimer >= 0)
  {
    killTimer(m_profileTimer);
    m_profileTimer = -1;
  }
}

namespace
{
QString nodeTypeName(const char* mangled)
{
  if(!mangled)
    return QObject::tr("Render node");
  QString name;
#if __has_include(<cxxabi.h>)
  int status = 0;
  char* d = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
  if(d && status == 0)
    name = QString::fromUtf8(d);
  std::free(d);
#endif
  if(name.isEmpty())
    name = QString::fromUtf8(mangled);
  name.remove(QStringLiteral("score::gfx::"));
  name.remove(QStringLiteral("Gfx::"));
  return name;
}
}

void DocumentPlugin::timerEvent(QTimerEvent*)
{
  pullProfile();
}

void DocumentPlugin::pullProfile()
{
  auto frames = context.profiler().take();
  if(frames.empty())
    return;

  auto& exec_plug = score::DocumentPlugin::context().plugin<Execution::DocumentPlugin>();
  ossia::hash_map<int32_t, const Process::ProcessModel*> procOfNode;
  for(const auto& [node, proc] : exec_plug.contextData()->setupContext.proc_map)
    if(auto gfx = dynamic_cast<const gfx_exec_node*>(node))
      procOfNode[gfx->id] = proc;

  std::vector<Execution::RenderFrameReport> reports;
  reports.reserve(frames.size());
  std::unordered_map<const char*, QString> names;
  for(auto& f : frames)
  {
    auto& r = reports.emplace_back();
    r.output = f.list;
    r.frame = f.frame;
    r.cpuNs = f.cpu_ns;
    r.gpuNs = f.gpu_ns;
    r.nodes.reserve(f.nodes.size());
    for(const auto& n : f.nodes)
    {
      auto& rn = r.nodes.emplace_back();
      if(auto it = procOfNode.find(n.node); it != procOfNode.end())
        rn.process = it->second;
      rn.key = (quintptr(uint32_t(n.node)) << 1) | 1;
      if(!rn.process)
      {
        auto [it, inserted] = names.try_emplace(n.type);
        if(inserted)
          it->second = nodeTypeName(n.type);
        rn.name = it->second;
      }
      rn.cpuNs = n.cpu_ns;
      rn.gpuNs = n.gpu_ns;
      rn.targetBytes = n.target_bytes;
    }
  }
  exec_plug.profiler().pushRender(reports);
}


ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
  // Early: the canvas watchers have to be in place before a context can be lost.
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  GUIElements e;

  auto bar = new QToolBar{QObject::tr("Graphics")};

  auto preview_act = new QAction{QObject::tr("Show shader previews"), bar};
  preview_act->setCheckable(true);
  preview_act->setChecked(Process::PreviewSettings::instance().enabled());
  score::setHelp(
      preview_act,
      QObject::tr("Render the shader previews in the library and the inspector"));
  setIcons(
      preview_act, QStringLiteral(":/icons/shader_preview_on.png"),
      QStringLiteral(":/icons/shader_preview_hover.png"),
      QStringLiteral(":/icons/shader_preview_off.png"),
      QStringLiteral(":/icons/shader_preview_disabled.png"));

  QObject::connect(preview_act, &QAction::toggled, preview_act, [](bool checked) {
    Process::PreviewSettings::instance().setEnabled(checked);
  });

  auto probe_act = new QAction{QObject::tr("Show port previews on hover"), bar};
  probe_act->setCheckable(true);
  probe_act->setChecked(Process::PreviewSettings::instance().hoverProbes());
  score::setHelp(
      probe_act, QObject::tr("Preview what goes through a port or a cable on hover"));
  setIcons(
      probe_act, QStringLiteral(":/icons/hover_probe_on.png"),
      QStringLiteral(":/icons/hover_probe_hover.png"),
      QStringLiteral(":/icons/hover_probe_off.png"),
      QStringLiteral(":/icons/hover_probe_disabled.png"));

  QObject::connect(probe_act, &QAction::toggled, probe_act, [](bool checked) {
    Process::PreviewSettings::instance().setHoverProbes(checked);
  });
  QObject::connect(
      &Process::PreviewSettings::instance(), &Process::PreviewSettings::hoverProbesChanged,
      probe_act, &QAction::setChecked);

  // The toggles are view switches, not edition tools: they take the far end
  // of the row instead of sitting in the tools' run.
  auto spacer = new QWidget{bar};
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  bar->addWidget(spacer);
  bar->addAction(probe_act);
  bar->addAction(preview_act);
  bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  score::setSkinIconSize(bar, 24);

  e.toolbars.emplace_back(
      bar, StringKey<score::Toolbar>("Graphics"), Qt::TopToolBarArea, 900);

  return e;
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DocumentPlugin{doc.context(), &doc.model()});
}

}
