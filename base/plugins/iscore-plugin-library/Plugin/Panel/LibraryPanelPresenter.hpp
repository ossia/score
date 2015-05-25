#pragma once
#include <iscore/plugins/panel/PanelPresenter.hpp>

class LibraryPanelPresenter : public iscore::PanelPresenter
{
    public:
        LibraryPanelPresenter(iscore::Presenter* parent,
                                iscore::PanelView* view);

        int panelId() const override;

        virtual void on_modelChanged() override;

    private:
        QMetaObject::Connection m_mvConnection;
};
