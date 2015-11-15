#pragma once
#include <Process/ProcessContext.hpp>

template<typename Tool_T, typename ToolPalette_T, typename Input_T>
class ToolPaletteInputDispatcher : public QObject
{
    public:
        ToolPaletteInputDispatcher(
                Input_T& input,
                ToolPalette_T& palette,
                LayerContext& context):
            m_palette{palette},
            m_context{context}
        {
            using EditionSettings_T = std::remove_reference_t<decltype(palette.editionSettings())>;
            con(palette.editionSettings(), &EditionSettings_T::toolChanged,
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
            if(m_running)
            {
                m_palette.on_cancel();
                m_palette.on_pressed(m_currentPoint);
            }
        }

        void on_pressed(QPointF p)
        {
            m_context.focusDispatcher.focus(&m_context.layerPresenter);
            m_currentPoint = p;
            m_palette.on_pressed(p);
            m_running = true;
        }

        void on_moved(QPointF p)
        {
            m_currentPoint = p;
            m_palette.on_moved(p);
        }

        void on_released(QPointF p)
        {
            m_currentPoint = p;
            m_palette.on_released(p);
            m_running = false;
        }

        void on_cancel()
        {
            m_palette.on_cancel();
            m_running = false;
        }

    private:
        ToolPalette_T& m_palette;
        LayerContext& m_context;
        QPointF m_currentPoint;
        bool m_running = false;
};
