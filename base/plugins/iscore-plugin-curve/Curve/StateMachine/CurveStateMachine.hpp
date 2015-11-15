#pragma once
#include <Curve/StateMachine/States/Tools/MoveTool.hpp>
#include <Curve/StateMachine/States/Tools/SelectionTool.hpp>
#include <Curve/StateMachine/CurveStateMachineData.hpp>
#include <Curve/StateMachine/CurvePoint.hpp>
#include <Curve/StateMachine/CurveEditionSettings.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>
#include <iscore/locking/ObjectLocker.hpp>

#include <core/command/CommandStack.hpp>

class CurvePresenter;
class QGraphicsScene;
class CurveModel;

namespace Curve
{
template<typename Tool_T, typename ToolPalette_T, typename Input_T>
class ToolPaletteInputDispatcher : public QObject
{
    public:
        ToolPaletteInputDispatcher(Input_T& input, ToolPalette_T& p):
            m_palette{p}
        {
            con(p.editionSettings(), &EditionSettings::toolChanged,
                this, &ToolPaletteInputDispatcher::on_toolChanged);
            con(input, &Input_T::pressed,
                this, &ToolPaletteInputDispatcher::on_pressed);
            con(input, &Input_T::moved,
                this, &ToolPaletteInputDispatcher::on_moved);
            con(input, &Input_T::released,
                this, &ToolPaletteInputDispatcher::on_released);
            con(input, &Input_T::escPressed,
                this, &ToolPaletteInputDispatcher::on_cancel);
        }

        void on_toolChanged(Tool_T t)
        {
            m_palette.on_cancel();
        }

        void on_pressed(QPointF p)
        {
            m_palette.on_pressed(p);
        }

        void on_moved(QPointF p)
        {
            m_palette.on_moved(p);
        }

        void on_released(QPointF p)
        {
            m_palette.on_released(p);
        }

        void on_cancel()
        {
            m_palette.on_cancel();
        }

    private:
        ToolPalette_T& m_palette;
};

class EditionToolForSelection;
class ToolPalette final : public GraphicsSceneToolPalette
{
        Q_OBJECT
    public:
        ToolPalette(CurvePresenter& pres, QObject* parent);
        CurvePresenter& presenter() const;
        Curve::EditionSettings& editionSettings() const;

        const CurveModel& model() const;

        iscore::CommandStack& commandStack() const;
        iscore::ObjectLocker& locker() const;

        Curve::Point curvePoint;

        void on_pressed(QPointF);
        void on_moved(QPointF);
        void on_released(QPointF);
        void on_cancel();

    private:
        Curve::Point ScenePointToCurvePoint(const QPointF& point);

        CurvePresenter& m_presenter;

        iscore::CommandStack& m_stack;
        iscore::ObjectLocker& m_locker;

        SelectionAndMoveTool m_selectTool;
        CreateTool m_createTool;
        SetSegmentTool m_setSegmentTool;
};
}
