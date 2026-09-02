// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessFactory.hpp"

#include <Process/HeaderDelegate.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/MissingProcess.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <QPainter>
#include <QTextOption>

#include <wobjectimpl.h>
namespace Process
{
ProcessModelFactory::~ProcessModelFactory() { }

Descriptor ProcessModelFactory::descriptor(const ProcessModel& m) const noexcept
{
  return descriptor(m.effect());
}

LayerFactory::~LayerFactory() { }

std::optional<double> LayerFactory::recommendedHeight() const noexcept
{
  return std::nullopt;
}

ProcessFactoryList::~ProcessFactoryList() { }

LayerFactoryList::~LayerFactoryList() { }

class DefaultLayerView final : public LayerView
{
public:
  DefaultLayerView(QGraphicsItem* parent)
      : LayerView(parent)
  {
  }
  void paint_impl(QPainter* p) const override
  {
    QTextOption o;
    o.setAlignment(Qt::AlignCenter);
    p->setPen(Qt::white);
    p->drawText(boundingRect(), m_txt, o);
  }
  QString m_txt;
};

class DefaultLayerPresenter final : public LayerPresenter
{
  Process::LayerView* m_view{};

public:
  DefaultLayerPresenter(
      const Process::ProcessModel& model, Process::LayerView* v, const Context& ctx,
      QObject* parent)
      : LayerPresenter{model, v, ctx, parent}
      , m_view{v}
  {
    auto vi = dynamic_cast<DefaultLayerView*>(v);
    vi->m_txt = model.metadata().getName();
    connect(&model.metadata(), &score::ModelMetadata::NameChanged, this, [=](auto t) {
      vi->m_txt = t;
      vi->update();
    });
  }

  ~DefaultLayerPresenter() override { }

  void setWidth(qreal width, qreal defaultWidth) override { m_view->setWidth(width); }
  void setHeight(qreal height) override { m_view->setHeight(height); }

  void putToFront() override { m_view->setVisible(true); }
  void putBehind() override { m_view->setVisible(false); }

  void on_zoomRatioChanged(ZoomRatio) override { }
  void parentGeometryChanged() override { }
};
LayerPresenter* LayerFactory::makeLayerPresenter(
    const ProcessModel& m, LayerView* v, const Context& context, QObject* parent) const
{
  return new DefaultLayerPresenter{m, v, context, parent};
}

LayerView* LayerFactory::makeLayerView(
    const ProcessModel& view, const Process::Context& context,
    QGraphicsItem* parent) const
{
  return new DefaultLayerView{parent};
}

Process::MiniLayer*
LayerFactory::makeMiniLayer(const ProcessModel& view, QGraphicsItem* parent) const
{
  return nullptr;
}

score::ResizeableItem* LayerFactory::makeItem(
    const ProcessModel&, const Process::Context& ctx, QGraphicsItem* parent) const
{
  return nullptr;
}

bool LayerFactory::hasCodeEditor(
    const ProcessModel&, const score::DocumentContext& ctx) const noexcept
{
  return false;
}
QWidget* LayerFactory::makeCodeEditor(
    const ProcessModel&, const score::DocumentContext& ctx, QWidget* parent) const
{
  return nullptr;
}

QWidget* LayerFactory::makeScriptUI(
    ProcessModel&, const score::DocumentContext& ctx, QWidget* parent) const
{
  return nullptr;
}

bool LayerFactory::hasExternalUI(
    const ProcessModel&, const score::DocumentContext& ctx) const noexcept
{
  return false;
}
QWidget* LayerFactory::makeExternalUI(
    ProcessModel&, const score::DocumentContext& ctx, QWidget* parent) const
{
  return nullptr;
}

HeaderDelegate* LayerFactory::makeHeaderDelegate(
    const ProcessModel& model, const Process::Context& ctx, QGraphicsItem* parent) const
{
  return new DefaultHeaderDelegate{model, ctx};
}
FooterDelegate* LayerFactory::makeFooterDelegate(
    const ProcessModel& model, const Process::Context& ctx) const
{
  return new DefaultFooterDelegate{model, ctx};
}

bool LayerFactory::matches(const ProcessModel& p) const
{
  return matches(p.concreteKey());
}

bool LayerFactory::matches(const UuidKey<Process::ProcessModel>& p) const
{
  return false;
}

ProcessFactoryList::object_type* ProcessFactoryList::loadMissing(
    const VisitorVariant& vis, const score::DocumentContext& ctx, QObject* parent) const
{
  // A process whose factory is not installed in this build. Returning nullptr
  // here (which is what this did) makes the loader drop the process AND every
  // cable attached to it, silently, while the document still reports as loaded
  // — and the next save makes the loss permanent. Keep it instead: see
  // Process::MissingProcess.
  switch(vis.identifier)
  {
    case JSONObject::type(): {
      auto& des = static_cast<JSONObject::Deserializer&>(vis.visitor);
      return new Process::MissingProcess{des, parent};
    }
    case DataStream::type(): {
      auto& des = static_cast<DataStream::Deserializer&>(vis.visitor);
      auto proc = new Process::MissingProcess{des, parent};
      if(proc->concreteKey() == UuidKey<Process::ProcessModel>{})
      {
        // The tail was written by the real process, not by us, so its uuid is
        // unrecoverable: deserialize_interface consumed the abstract key before
        // calling us and the DataStream deserializer cannot rewind. Inventing an
        // identity would be worse than declining. Only reachable through the
        // clipboard / undo stack — documents are JSON.
        qWarning() << "Process::loadMissing: cannot preserve a binary-serialized "
                      "process whose factory is missing";
        delete proc;
        return nullptr;
      }
      return proc;
    }
    default:
      return nullptr;
  }
}

//! Stand-in for a process whose layer factory is not installed. Everything it
//! does is LayerFactory's default behaviour; it exists only so that
//! findDefaultFactory never returns null.
class MissingLayerFactory final : public Process::LayerFactory
{
  SCORE_CONCRETE("1cc0e9e2-6b16-4b8a-a1a1-6c2e6b6e0d4f")

  //! Never registered in the interface list; it is only ever handed out
  //! explicitly by findDefaultFactory, so it claims nothing.
  bool matches(const UuidKey<Process::ProcessModel>&) const override { return false; }
};

LayerFactory* LayerFactoryList::findDefaultFactory(const ProcessModel& proc) const
{
  return findDefaultFactory(proc.concreteKey());
}

LayerFactory*
LayerFactoryList::findDefaultFactory(const UuidKey<ProcessModel>& proc) const
{
  for(auto& fac : *this)
  {
    if(fac.matches(proc))
      return &fac;
  }

  // No factory claims this key. That used to be impossible in practice, because
  // a process with no factory never made it past loading; now Process::
  // MissingProcess does, and the scenario presenters (TemporalIntervalPresenter,
  // FullViewIntervalPresenter, LayerData::updateLoops) dereference this pointer
  // unconditionally. Hand back a factory whose every method is the base-class
  // default: the layer is the standard "name in a box" DefaultLayerView.
  static MissingLayerFactory missing;
  return &missing;
}

QString ProcessModelFactory::customConstructionData() const noexcept
{
  return {};
}
}
