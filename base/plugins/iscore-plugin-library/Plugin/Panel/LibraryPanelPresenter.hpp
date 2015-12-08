#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

class LibraryPanelPresenter : public iscore::PanelPresenter
{
    public:
        LibraryPanelPresenter(
                iscore::PanelView* view,
                QObject* parent);

        int panelId() const override;

        void on_modelChanged() override;

    private:
        QMetaObject::Connection m_mvConnection;
};
