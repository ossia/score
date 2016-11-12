#pragma once
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Palette/Tools/MoveTool.hpp>
#include <Curve/Palette/Tools/SmartTool.hpp>
#include <Process/Tools/ToolPalette.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <QPoint>
#include <iscore_plugin_curve_export.h>
namespace iscore {
class CommandStackFacade;
class ObjectLocker;
}  // namespace iscore
namespace Process
{
struct LayerContext;
}

namespace Curve
{
class Model;
class Presenter;
class View;

class ToolPalette : public GraphicsSceneToolPalette
{
    public:
        ToolPalette(const iscore::DocumentContext& ctx, Presenter& pres):
            GraphicsSceneToolPalette{*pres.view().scene()},
            m_presenter{pres},
            m_selectTool{*this, ctx},
            m_createTool{*this, ctx},
            m_setSegmentTool{*this, ctx}
        {
        }

        Presenter& presenter() const
        {
            return m_presenter;
        }

        Curve::EditionSettings& editionSettings() const
        {
            return m_presenter.editionSettings();
        }

        const Model& model() const
        {
            return m_presenter.model();
        }

        void on_pressed(QPointF point)
        {
            scenePoint = point;
            auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
            switch(editionSettings().tool())
            {
                case Curve::Tool::Create:
                    m_createTool.on_pressed(point, curvePoint);
                    break;
                case Curve::Tool::Select:
                    m_selectTool.on_pressed(point, curvePoint);
                    break;
                case Curve::Tool::SetSegment:
                    m_setSegmentTool.on_pressed(point, curvePoint);
                    break;
                default:
                    break;
            }
        }

        void on_moved(QPointF point)
        {
            scenePoint = point;
            auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
            switch(editionSettings().tool())
            {
                case Curve::Tool::Create:
                    m_createTool.on_moved(point, curvePoint);
                    break;
                case Curve::Tool::Select:
                    m_selectTool.on_moved(point, curvePoint);
                    break;
                case Curve::Tool::SetSegment:
                    m_setSegmentTool.on_moved(point, curvePoint);
                    break;
                default:
                    break;
            }
        }

        void on_released(QPointF point)
        {
            scenePoint = point;
            auto curvePoint = ScenePointToCurvePoint(m_presenter.view().mapFromScene(point));
            switch(editionSettings().tool())
            {
                case Curve::Tool::Create:
                    m_createTool.on_released(point, curvePoint);
                    break;
                case Curve::Tool::Select:
                    m_selectTool.on_released(point, curvePoint);
                    break;
                case Curve::Tool::SetSegment:
                    m_setSegmentTool.on_released(point, curvePoint);
                    break;
                default:
                    break;
            }
        }

        void on_cancel()
        {
            m_createTool.on_cancel();
            m_selectTool.on_cancel();
            m_setSegmentTool.on_cancel();
        }

        void activate(Curve::Tool)
        {

        }
        void desactivate(Curve::Tool)
        {

        }

    private:
        Curve::Point ScenePointToCurvePoint(const QPointF& point)
        {
            const auto rect = m_presenter.rect();
            return {point.x() / rect.width(),
                        1. - point.y() / rect.height()};
        }

        Presenter& m_presenter;

        SmartTool m_selectTool;
        CreateTool m_createTool;
        SetSegmentTool m_setSegmentTool;
};

template<typename Context_T>
struct ToolPalette_T final : public ToolPalette
{
        ToolPalette_T(Context_T& ctx, Presenter& pres):
            ToolPalette{ctx.context, pres},
            m_inputDisp{pres.view(), *this, ctx}
        {

        }

        ToolPaletteInputDispatcher<
          Curve::Tool,
          ToolPalette,
          Context_T,
          View> m_inputDisp;
};

}
