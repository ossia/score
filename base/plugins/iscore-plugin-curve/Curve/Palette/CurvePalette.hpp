#pragma once
#include <Curve/Palette/Tools/MoveTool.hpp>
#include <Curve/Palette/Tools/SmartTool.hpp>
#include <Curve/Palette/CurvePaletteData.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>

#include <Process/Focus/FocusDispatcher.hpp>
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <iscore/locking/ObjectLocker.hpp>

#include <core/command/CommandStack.hpp>
#include <Process/Tools/ToolPalette.hpp>

class CurvePresenter;
class QGraphicsScene;
class CurveModel;
class CurveView;
class FocusDispatcher;


namespace Curve
{
class ToolPalette final : public GraphicsSceneToolPalette
{
        Q_OBJECT
    public:
        ToolPalette(LayerContext& f, CurvePresenter& pres);

        CurvePresenter& presenter() const;
        Curve::EditionSettings& editionSettings() const;

        const CurveModel& model() const;

        iscore::CommandStack& commandStack() const;
        iscore::ObjectLocker& locker() const;

        void on_pressed(QPointF);
        void on_moved(QPointF);
        void on_released(QPointF);
        void on_cancel();

        void activate(Curve::Tool);
        void desactivate(Curve::Tool);

    private:
        Curve::Point ScenePointToCurvePoint(const QPointF& point);

        CurvePresenter& m_presenter;

        iscore::CommandStack& m_stack;
        iscore::ObjectLocker& m_locker;

        SmartTool m_selectTool;
        CreateTool m_createTool;
        SetSegmentTool m_setSegmentTool;

        ToolPaletteInputDispatcher<
            Curve::Tool,
            ToolPalette,
            LayerContext,
            CurveView> m_inputDisp;
};
}
