#pragma once
#include <iscore/plugins/panel/PanelView.hpp>

#include <QTabWidget>

namespace Library
{
class LibraryPanelView : public iscore::PanelView
{
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;
        LibraryPanelView(QObject* parent);

        QWidget* getWidget() override;
        const QString shortcut() const override
        { return tr("Ctrl+L"); }

    private:
        QTabWidget* m_widget{};
};
}
