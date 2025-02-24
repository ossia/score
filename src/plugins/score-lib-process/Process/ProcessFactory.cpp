// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessFactory.hpp"

#include <Process/HeaderDelegate.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
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

bool LayerFactory::hasExternalUI(
    const ProcessModel&, const score::DocumentContext& ctx) const noexcept
{
  return false;
}
QWidget* LayerFactory::makeExternalUI(
    const ProcessModel&, const score::DocumentContext& ctx, QWidget* parent) const
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
  SCORE_TODO;
  return nullptr;
}

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
  return nullptr;
}

QString ProcessModelFactory::customConstructionData() const noexcept
{
  return {};
}
}
