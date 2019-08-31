#include "score_addon_nodal.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <Nodal/CommandFactory.hpp>
#include <Nodal/Executor.hpp>
#include <Nodal/Inspector.hpp>
#include <Nodal/Layer.hpp>
#include <Nodal/LocalTree.hpp>
#include <Nodal/Process.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score_addon_nodal_commands_files.hpp>

#include <Process/Style/ScenarioStyle.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QHBoxLayout>
namespace Nodal
{
class Panel final : public QGraphicsView
{
public:
  QGraphicsScene m_scene;
  Panel(const score::DocumentContext& ctx, QWidget* parent)
      : QGraphicsView{parent}
      , m_ctx{ctx, m_focusDispatcher}
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
    auto focus = reinterpret_cast<const score::FocusManager*const *>(&ctx.focus);
    connect(*focus, &score::FocusManager::changed,
            this, [&] {
      delete m_item;
      m_item = nullptr;

      if(auto focus = ctx.focus.get())
      {
        m_item = new score::EmptyRectItem{nullptr};
        m_scene.addItem(m_item);

        auto parent = dynamic_cast<Scenario::IntervalModel*>(focus->parent());

        int i = 0;

        for(auto node : m_nodes)
        {
          node->release();
          delete node;
        }

        m_nodes.clear();
        m_nodeItems.clear();

        for(auto& proc : parent->processes)
        {
          auto node = new Node{
              Node::no_ownership{}
              , proc
              , Id<Node>{i++}
              , &ctx.document.model().modelDelegate()};

          m_nodes.push_back(node);

          auto item = new NodeItem{*node, m_ctx, m_item};
          m_nodeItems.push_back(item);
          item->setZoomRatio(1000);
        }


      }
    });
  }

  Process::LayerPresenter* m_presenter{};
  FocusDispatcher m_focusDispatcher;
  Process::ProcessPresenterContext m_ctx;
  QGraphicsItem* m_item{};
  std::vector<Nodal::Node*> m_nodes;
  std::vector<Nodal::NodeItem*> m_nodeItems;
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


score_addon_nodal::score_addon_nodal()
{
}

score_addon_nodal::~score_addon_nodal()
{
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_addon_nodal::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Nodal::ProcessFactory>,
      FW<Process::LayerFactory, Nodal::LayerFactory>,
      FW<Process::InspectorWidgetDelegateFactory, Nodal::InspectorFactory>,
      FW<Execution::ProcessComponentFactory,
         Nodal::ProcessExecutorComponentFactory>,
      FW<score::ObjectRemover, Nodal::NodeRemover>,
      FW<score::PanelDelegateFactory, Nodal::PanelDelegateFactory>
      //, FW<LocalTree::ProcessComponentFactory,
      //   Nodal::LocalTreeProcessComponentFactory>
      >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_addon_nodal::make_commands()
{
  using namespace Nodal;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Nodal::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QScrollArea>
#include <score_addon_nodal_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_nodal)
