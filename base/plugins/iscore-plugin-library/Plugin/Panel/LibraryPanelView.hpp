#pragma once
#include <iscore/plugins/panel/PanelView.hpp>

#include <QTabWidget>
namespace iscore
{
    class View;
}

class LibraryPanelView : public iscore::PanelView
{
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;
        LibraryPanelView(iscore::View* parent);

        QWidget* getWidget();
	const QString shortcut() const override
	{ return tr("Ctrl+L"); }

    private:
        QTabWidget* m_widget{};
};
