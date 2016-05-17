#pragma once
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Palette/Tools/MoveTool.hpp>
#include <Curve/Palette/Tools/SmartTool.hpp>
#include <Process/Tools/ToolPalette.hpp>
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
class ISCORE_PLUGIN_CURVE_EXPORT ToolPalette final : public GraphicsSceneToolPalette
{
        Q_OBJECT
    public:
        ToolPalette(Process::LayerContext& f, Presenter& pres);

        Presenter& presenter() const;
        Curve::EditionSettings& editionSettings() const;

        const Model& model() const;

        const Process::LayerContext& context() const
        { return m_context; }

        void on_pressed(QPointF);
        void on_moved(QPointF);
        void on_released(QPointF);
        void on_cancel();

        void activate(Curve::Tool);
        void desactivate(Curve::Tool);

    private:
        Curve::Point ScenePointToCurvePoint(const QPointF& point);

        Presenter& m_presenter;

        const Process::LayerContext& m_context;

        SmartTool m_selectTool;
        CreateTool m_createTool;
        SetSegmentTool m_setSegmentTool;

        ToolPaletteInputDispatcher<
            Curve::Tool,
            ToolPalette,
            Process::LayerContext,
            View> m_inputDisp;
};
}
