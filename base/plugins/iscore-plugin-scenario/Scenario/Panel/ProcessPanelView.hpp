#pragma once
#include <iscore/plugins/panel/PanelView.hpp>

class QObject;
class QVBoxLayout;
class QWidget;

namespace Scenario
{
class ProcessPanelView final : public iscore::PanelView
{
        Q_OBJECT
        static const iscore::DefaultPanelStatus m_status;
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;

        ProcessPanelView(QObject* parent);

        void setInnerWidget(QWidget* widg);

        QWidget* getWidget() override;
        const QString shortcut() const override
        { return tr("Ctrl+P"); }

    private:
        QWidget* m_widget{};
        QVBoxLayout* m_lay{};
};
}
