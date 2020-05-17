#include "score_addon_nodal.hpp"

#include <Process/DocumentPlugin.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>

#include <Nodal/CommandFactory.hpp>
#include <Nodal/Executor.hpp>
#include <Nodal/Layer.hpp>
#include <Nodal/LocalTree.hpp>
#include <Nodal/Process.hpp>
#include <score_addon_nodal_commands_files.hpp>
/*
namespace Nodal
{
class Panel final
    : public QGraphicsView
    , public Nano::Observer
{
public:
  QGraphicsScene m_scene;


  Panel(const score::DocumentContext& ctx, QWidget* parent)
      : QGraphicsView{parent}
      , m_context{ctx, m_dataflow, m_focusDispatcher}
  {
    auto& skin = score::Skin::instance();
    con(skin, &score::Skin::changed, this, [this] {
      auto& skin = Process::Style::instance();
      setBackgroundBrush(skin.Background());
    });
    setBackgroundBrush(Process::Style::instance().Background());
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setRenderHints(
        QPainter::Antialiasing | QPainter::SmoothPixmapTransform
        | QPainter::TextAntialiasing);

    setFrameStyle(0);
    // setCacheMode(QGraphicsView::CacheBackground);
    setDragMode(QGraphicsView::NoDrag);

  #if !defined(__EMSCRIPTEN__)
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
  #endif

    setScene(&m_scene);


    auto& cables =
safe_cast<Scenario::ScenarioDocumentModel&>(ctx.document.model().modelDelegate()).cables;
    cables.mutable_added.connect<&Panel::on_cableAdded>(*this);
    cables.removing.connect<&Panel::on_cableRemoving>(*this);

    auto focus = reinterpret_cast<const score::FocusManager*const
*>(&ctx.focus); connect(*focus, &score::FocusManager::changed, this,
&Panel::on_focusChanged);
  }

  void on_focusChanged()
  {
    delete m_item;
    m_item = nullptr;

    if(auto focus = m_context.focus.get())
    {
      m_item = new score::EmptyRectItem{nullptr};
      m_scene.addItem(m_item);

      auto parent = dynamic_cast<Scenario::IntervalModel*>(focus->parent());
      if(!parent)
        return;


      m_nodes.clear();
      m_nodeItems.clear();

      for(auto& proc : parent->processes)
      {
        m_nodes.push_back(&proc);

        auto item = new Process::NodeItem{proc, m_context, m_item};
        m_nodeItems.push_back(item);
        item->setZoomRatio(item->width());
      }
    }
  }

  void on_cableAdded(Process::Cable& c)
  {
    // TODO only show cables that are in this interval
    auto it = new Dataflow::CableItem{c, m_context, nullptr};
    m_scene.addItem(it);
  }

  void on_cableRemoving(const Process::Cable& c)
  {
    auto it = m_dataflow.cables().find(const_cast<Process::Cable*>(&c));
    if(it != m_dataflow.cables().end())
    {
      delete it->second;
      m_dataflow.cables().erase(it);
    }
  }

  Process::LayerPresenter* m_presenter{};
  Process::DataflowManager m_dataflow;
  FocusDispatcher m_focusDispatcher;
  Process::Context m_context;
  QGraphicsItem* m_item{};
  std::vector<Process::ProcessModel*> m_nodes;
  std::vector<Process::NodeItem*> m_nodeItems;
};
class PanelDelegate final : public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new QWidget}
  {
    m_widget->setLayout(new score::MarginLess<QHBoxLayout>);
  }

  QWidget* widget() override
  { return m_widget; }

private:
  const score::PanelStatus& defaultPanelStatus() const override
  {
    static const score::PanelStatus status{true, false,
                                           Qt::BottomDockWidgetArea,
                                           10,
                                           QObject::tr("Nodal"),
                                           QObject::tr("Ctrl+Alt+N")};
    return status;
  }

  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
      override
  {
    delete m_cur;
    m_cur = nullptr;

    if (newm)
    {
      m_cur = new Panel{*newm, m_widget};
      m_widget->layout()->addWidget(m_cur);
    }
  }

  QWidget* m_widget{};
  Panel* m_cur{};
};

class PanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("a1066c88-7def-4fa9-b048-674a69eda981")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override
  {  return std::make_unique<Nodal::PanelDelegate>(ctx); }
};
}

*/
score_addon_nodal::score_addon_nodal() { }

score_addon_nodal::~score_addon_nodal() { }

std::vector<std::unique_ptr<score::InterfaceBase>> score_addon_nodal::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Nodal::ProcessFactory>,
      FW<Process::LayerFactory, Nodal::LayerFactory>,
      FW<Execution::ProcessComponentFactory, Nodal::ProcessExecutorComponentFactory>,
      FW<score::ObjectRemover, Nodal::NodeRemover>
      //, FW<score::PanelDelegateFactory, Nodal::PanelDelegateFactory>
      //, FW<LocalTree::ProcessComponentFactory,
      //   Nodal::LocalTreeProcessComponentFactory>
      >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_addon_nodal::make_commands()
{
  using namespace Nodal;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Nodal::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_addon_nodal_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_nodal)
