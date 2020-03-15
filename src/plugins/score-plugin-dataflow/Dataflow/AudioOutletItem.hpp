#pragma once
#include <score_plugin_dataflow_export.h>
#include <Dataflow/PortItem.hpp>
#include <Process/Dataflow/MinMaxFloatPort.hpp>
#include <score/graphics/ArrowDialog.hpp>
#include <QPointer>
namespace Dataflow
{
class SCORE_PLUGIN_DATAFLOW_EXPORT AudioOutletItem : public AutomatablePortItem
{
public:
  AudioOutletItem(
      Process::Port& p,
      const Process::Context& ctx,
      QGraphicsItem* parent);
  ~AudioOutletItem() override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QVariant
  itemChange(GraphicsItemChange change, const QVariant& value) override;

  QPointer<score::ArrowDialog> m_subView{};
};

struct SCORE_PLUGIN_DATAFLOW_EXPORT AudioOutletFactory final : public AutomatablePortFactory
{
    using Model_T = Process::AudioOutlet;
    UuidKey<Process::Port> concreteKey() const noexcept override
    {
        return Metadata<ConcreteKey_k, Model_T>::get();
    }

    Model_T* load(const VisitorVariant& vis, QObject* parent) override
    {
        return score::deserialize_dyn(vis, [&](auto&& deserializer) {
            return new Model_T{deserializer, parent};
        });
    }

    void setupOutletInspector(
            Process::Outlet& port,
            const score::DocumentContext& ctx,
            QWidget* parent,
            Inspector::Layout& lay,
            QObject* context) override;

    PortItem* makeItem(Process::Outlet& port, const Process::Context& ctx, QGraphicsItem* parent, QObject* context) override
    {
      return new Dataflow::AudioOutletItem{port, ctx, parent};
    }
};

class SCORE_PLUGIN_DATAFLOW_EXPORT MinMaxFloatOutletItem : public AutomatablePortItem
{
public:
  MinMaxFloatOutletItem(
      Process::Port& p,
      const Process::Context& ctx,
      QGraphicsItem* parent);
  ~MinMaxFloatOutletItem() override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QVariant
  itemChange(GraphicsItemChange change, const QVariant& value) override;

  QPointer<score::ArrowDialog> m_subView{};
};

struct SCORE_PLUGIN_DATAFLOW_EXPORT MinMaxFloatOutletFactory final : public AutomatablePortFactory
{
    using Model_T = Process::MinMaxFloatOutlet;
    UuidKey<Process::Port> concreteKey() const noexcept override
    {
        return Metadata<ConcreteKey_k, Model_T>::get();
    }

    Model_T* load(const VisitorVariant& vis, QObject* parent) override
    {
        return score::deserialize_dyn(vis, [&](auto&& deserializer) {
            return new Model_T{deserializer, parent};
        });
    }

    void setupOutletInspector(
            Process::Outlet& port,
            const score::DocumentContext& ctx,
            QWidget* parent,
            Inspector::Layout& lay,
            QObject* context) override;

    PortItem* makeItem(Process::Outlet& port, const Process::Context& ctx, QGraphicsItem* parent, QObject* context) override
    {
      return new Dataflow::MinMaxFloatOutletItem{port, ctx, parent};
    }
};

}
