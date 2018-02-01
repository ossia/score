#pragma once
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Palette/Tools/MoveTool.hpp>
#include <Curve/Palette/Tools/SmartTool.hpp>
#include <Process/Tools/ToolPalette.hpp>
#include <QPoint>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>
#include <score_plugin_curve_export.h>
namespace score
{
class CommandStackFacade;
class ObjectLocker;
} // namespace score
namespace Process
{
struct LayerContext;
}

namespace Curve
{
class Model;
class Presenter;
class View;

class SCORE_PLUGIN_CURVE_EXPORT ToolPalette : public GraphicsSceneToolPalette
{
public:
  ToolPalette(const score::DocumentContext& ctx, Presenter& pres);
  ~ToolPalette();
  Presenter& presenter() const;

  Curve::EditionSettings& editionSettings() const;

  const Model& model() const;
  void on_pressed(QPointF point);
  void on_moved(QPointF point);
  void on_released(QPointF point);

  void on_cancel();

  void activate(Curve::Tool);
  void desactivate(Curve::Tool);

  // From double-click :
  void createPoint(QPointF);
private:
  Curve::Point ScenePointToCurvePoint(const QPointF& point)
  {
    const auto rect = m_presenter.rect();
    return {point.x() / rect.width(), 1. - point.y() / rect.height()};
  }

  Presenter& m_presenter;

  SmartTool m_selectTool;
  CreateTool m_createTool;
  SetSegmentTool m_setSegmentTool;
  CreatePenTool m_createPenTool;
};

template <typename Context_T>
struct ToolPalette_T final : public ToolPalette
{
  ToolPalette_T(Context_T& ctx, Presenter& pres)
      : ToolPalette{ctx.context, pres}, m_inputDisp{pres.view(), *this, ctx}
  {
  }

  ToolPaletteInputDispatcher<Curve::Tool, ToolPalette, Context_T, View>
      m_inputDisp;
};
}
