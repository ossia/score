#pragma once

#include <Process/Dataflow/Port.hpp>

#include <Dataflow/PortItem.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <QGraphicsItem>
#include <QTableWidget>

namespace Curve
{
class Model;
}
namespace Dataflow
{
struct CurveInlet;
}

UUID_METADATA(
    SCORE_PLUGIN_DATAFLOW_EXPORT, Process::Port, Dataflow::CurveInlet,
    "87421d82-9727-4faa-b053-a372ea5fd2df")

namespace WidgetFactory
{
struct CurveInletItems
{
  static constexpr Process::PortItemLayout layout() noexcept
  {
    using namespace Process;
    return {};
  }
  static QWidget* make_widget(
      const Dataflow::CurveInlet& inlet, const score::DocumentContext& ctx,
      QWidget* parent, QObject* context);

  static QGraphicsItem* make_item(
      const Dataflow::CurveInlet& slider, const Dataflow::CurveInlet& inlet,
      const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
};
}

namespace Dataflow
{
struct SCORE_PLUGIN_DATAFLOW_EXPORT CurveInlet : public Process::ControlInlet
{
  MODEL_METADATA_IMPL(CurveInlet)
  CurveInlet(Id<Process::Port> id, QObject* parent);
  ~CurveInlet();
  CurveInlet(DataStream::Deserializer& vis, QObject* parent);
  CurveInlet(JSONObject::Deserializer& vis, QObject* parent);
  CurveInlet(DataStream::Deserializer&& vis, QObject* parent);
  CurveInlet(JSONObject::Deserializer&& vis, QObject* parent);

  void init();

  //! Curve editor -> port value
  void on_curveChange();
  //! Port value -> curve editor, so that presets and any other external write
  //! to the control actually show up in the editor.
  void on_valueChange(const ossia::value& v);

  Curve::Model* m_curve{};

  //! Guards the two directions against each other: rebuilding the model emits
  //! changed(), which would otherwise immediately serialize it back.
  bool m_syncing{};
};
}
