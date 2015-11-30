#pragma once
#include <Process/ProcessContext.hpp>
#include <chrono>
#include <QTimer>
#include <QPointF>
#include <QScreen>
#include <QDebug>
#include <QApplication>
template<typename Tool_T, typename ToolPalette_T, typename Context_T, typename Input_T>
class ToolPaletteInputDispatcher : public QObject
{
    public:
        ToolPaletteInputDispatcher(
                const Input_T& input,
                ToolPalette_T& palette,
                Context_T& context):
            m_palette{palette},
            m_context{context},
            m_currentTool{palette.editionSettings().tool()}
        {
            auto screens = QApplication::screens();
            if(!screens.empty())
            {
                m_frameTime = 1000000. / screens.front()->refreshRate();
            }
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
            con(m_elapsedTimer, &QTimer::timeout,
                this, &ToolPaletteInputDispatcher::on_moved_from_timer);
        }

        void on_toolChanged(Tool_T t)
        {
            m_palette.desactivate(m_currentTool);
            m_palette.activate(t);
            m_currentTool = t;
            if(m_running)
            {
                m_palette.on_cancel();
                m_prev = std::chrono::steady_clock::now();
                m_palette.on_pressed(m_currentPoint);
            }
        }

        void on_pressed(QPointF p)
        {
            m_context.focusDispatcher.focus(&m_context.layerPresenter);
            m_currentPoint = p;
            m_prev = std::chrono::steady_clock::now();
            m_palette.on_pressed(p);
            m_running = true;
        }

        void on_moved(QPointF p)
        {
            using namespace std::literals::chrono_literals;
            auto t = std::chrono::steady_clock::now();
            if(t - m_prev < std::chrono::microseconds((int64_t)m_frameTime))
            {
                m_elapsedPoint = p;
                //m_elapsedTimer.start(m_frameTime / 1000.);
                // TODO here put a timer at refresh time ms to trigger the last step if we stop moving.
                //return;
            }
            m_currentPoint = p;
            m_palette.on_moved(p);
            m_prev = t;
        }

        void on_moved_from_timer()
        {
            if(m_running)
            {
                m_prev = std::chrono::steady_clock::now();
                m_currentPoint = m_elapsedPoint;
                m_palette.on_moved(m_elapsedPoint);
            }
        }


        void on_released(QPointF p)
        {
            m_running = false;
            m_elapsedTimer.stop();

            m_currentPoint = p;
            m_palette.on_released(p);
        }

        void on_cancel()
        {
            m_running = false;
            m_elapsedTimer.stop();

            m_palette.on_cancel();
        }

    private:
        ToolPalette_T& m_palette;
        Context_T& m_context;
        QPointF m_currentPoint;
        bool m_running = false;
        Tool_T m_currentTool;

        std::chrono::steady_clock::time_point m_prev;
        QTimer m_elapsedTimer;
        QPointF m_elapsedPoint;

        qreal m_frameTime{16666}; // In microseconds
};
