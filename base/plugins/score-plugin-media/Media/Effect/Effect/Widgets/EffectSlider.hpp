#pragma once
#include <QWidget>
#include <State/Address.hpp>
namespace Process
{
class ControlInlet;
}
namespace score
{
class DoubleSlider;
}

namespace Media
{
namespace Effect
{
class EffectSlider :
        public QWidget
{
    Q_OBJECT
    public:
        EffectSlider(Process::ControlInlet& fx, bool is_output, QWidget* parent);

        ~EffectSlider() override;

        double scaledValue;
        score::DoubleSlider* m_slider{};

    Q_SIGNALS:
        void createAutomation(const State::Address&, double min, double max);

    private:
        void contextMenuEvent(QContextMenuEvent* event) override;
        void on_paramDeleted();

        Process::ControlInlet& m_param;
        float m_min{0.};
        float m_max{1.};

        QAction* m_addAutomAction{};
};
}
}
