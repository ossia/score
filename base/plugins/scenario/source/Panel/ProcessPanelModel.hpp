#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>

class ProcessPanelModel : public iscore::PanelModel
{
    public:
        ProcessPanelModel(QObject*);
        int panelId() const override;

    public slots:
        void setNewSelection(const Selection&) override;
};
