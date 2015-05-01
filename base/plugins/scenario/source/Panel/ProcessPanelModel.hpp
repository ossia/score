#pragma once
#include <iscore/plugins/panel/PanelModelInterface.hpp>

class ProcessPanelModel : public iscore::PanelModelInterface
{
    public:
        ProcessPanelModel(QObject*);
        int panelId() const override;

    public slots:
        void setNewSelection(const Selection&) override;
};
