#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectLayout.hpp>
#include <Gfx/Video/Presenter.hpp>
#include <Gfx/Video/Process.hpp>
#include <Gfx/Video/View.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/detail/ssize.hpp>

#include <QFileInfo>
#include <QUrl>

namespace Gfx::Video
{
struct Presenter::Port
{
  score::EmptyRectItem* root;
  Dataflow::PortItem* port;
  QRectF rect;
};
Presenter::Presenter(
    const Model& layer, View* view, const Process::Context& ctx, QObject* parent)
    : Process::LayerPresenter{layer, view, ctx, parent}
    , m_view{view}
{
  auto& portFactory = ctx.app.interfaces<Process::PortFactoryList>();
  for(auto& e : layer.inlets())
  {
    auto inlet = qobject_cast<Process::ControlInlet*>(e);
    if(!inlet)
      continue;

    setupInlet(*inlet, portFactory, ctx);
  }

  connect(m_view, &View::dropReceived, this, &Presenter::on_drop);
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setOpacity(1);
}

void Presenter::putBehind()
{
  m_view->setOpacity(0.2);
}

void Presenter::on_zoomRatioChanged(ZoomRatio r)
{
  m_view->setZoom(r);
}

void Presenter::parentGeometryChanged() { }

void Presenter::setupInlet(
    Process::ControlInlet& port, const Process::PortFactoryList& portFactory,
    const Process::Context& doc)
{
  // Main item creation
  int i = std::ssize(m_ports);

  auto csetup = Process::controlSetup(
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
    return factory.makePortItem(inlet, doc, item, parent);
      },
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
    return factory.makeControlItem(inlet, doc, item, parent);
  },
       [&](int j) { return m_ports[j].rect.size(); }, [&] { return port.name(); });
  auto [item, portItem, widg, lab, itemRect]
      = Process::createControl(i, csetup, port, portFactory, doc, m_view, this);

  m_ports.push_back(Port{item, portItem, itemRect});
  // TODO updateRect();

  // TODO con(inlet, &Process::ControlInlet::domainChanged, this, [this,
  // &inlet] {
  // TODO   on_controlRemoved(inlet);
  // TODO   on_controlAdded(inlet.id());
  // TODO });
}

void Presenter::on_drop(const QPointF& pos, const QMimeData& md)
{
  if(md.urls().empty())
    return;
  const auto& handler = Video::DropHandler{};
  auto file = QFileInfo{md.urls().front().toLocalFile()};
  if(!handler.fileExtensions().contains(file.suffix().toLower()))
    return;

  auto path
      = score::relativizeFilePath(file.absoluteFilePath(), this->m_context.context);
  CommandDispatcher<> disp{m_context.context.commandStack};
  disp.submit<ChangeVideo>(static_cast<const Video::Model&>(model()), path);
}

}
