// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LayerPresenter.hpp"

#include "LayerView.hpp"

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::LayerPresenter)
W_OBJECT_IMPL(FocusDispatcher)
namespace Process
{
Context::Context(
    const score::DocumentContext& doc, DataflowManager& dfm, FocusDispatcher& d)
    : score::DocumentContext{doc}
    , dataflow{dfm}
    , focusDispatcher{d}
    , processList{doc.app.interfaces<Process::LayerFactoryList>()}
{
}

LayerPresenter::~LayerPresenter() = default;

LayerPresenter::LayerPresenter(
    const ProcessModel& model, const LayerView* view, const Context& ctx,
    QObject* parent)
    : QObject{parent}
    , m_context{ctx, *this}
    , m_process{model}
{
  connect(view, &LayerView::presetDropReceived, this, &LayerPresenter::handlePresetDrop);

  connect(
      view, &Process::LayerView::pressed, this, &Process::LayerPresenter::requestFocus);

  connect(
      view, &Process::LayerView::askContextMenu, this,
      &Process::LayerPresenter::contextMenuRequested);
}

void LayerPresenter::handlePresetDrop(const QPointF&, const QMimeData& mime)
{
  auto data = mime.data(score::mime::processpreset());
  auto& procs = m_context.context.app.interfaces<Process::ProcessFactoryList>();
  auto& load_preset_ifaces
      = this->m_context.context.app.interfaces<LoadPresetCommandFactoryList>();
  if(load_preset_ifaces.empty())
    return;
  if(auto preset = Process::Preset::fromJson(procs, data))
  {
    // TODO effect
    if(preset->key.key == m_process.concreteKey())
    {
      auto cmd = load_preset_ifaces.make(
          &LoadPresetCommandFactory::make, m_process, *preset, m_context.context);
      if(cmd)
        CommandDispatcher<>{m_context.context.commandStack}.submit(cmd);
    }
  }
}

void LayerPresenter::requestFocus()
{
  if(this->m_process.flags() & Process::ProcessFlags::ItemRequiresUniqueFocus)
    m_context.context.focusDispatcher.focus(this);
  score::SelectionDispatcher{m_context.context.selectionStack}.select(m_process);
}

bool LayerPresenter::focused() const
{
  return m_focus;
}

void LayerPresenter::setFocus(bool focus)
{
  m_focus = focus;
  on_focusChanged();
}

void LayerPresenter::on_focusChanged() { }

void LayerPresenter::setFullView() { }

const ProcessModel& LayerPresenter::model() const noexcept
{
  return m_process;
}

void LayerPresenter::fillContextMenu(
    QMenu&, QPoint pos, QPointF scenepos, const LayerContextMenuManager&)
{
}

GraphicsShapeItem* LayerPresenter::makeSlotHeaderDelegate()
{
  return nullptr;
}

GraphicsShapeItem::~GraphicsShapeItem() { }

void GraphicsShapeItem::setSize(QSizeF sz)
{
  prepareGeometryChange();
  m_sz = sz;
  update();
}

QRectF GraphicsShapeItem::boundingRect() const
{
  return {0, 0, m_sz.width(), m_sz.height()};
}
}
