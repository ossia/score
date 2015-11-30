#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>

#include <iscore/selection/Selection.hpp>

class QObject;

class ProcessPanelModel final : public iscore::PanelModel
{
    public:
        ProcessPanelModel(QObject*);
        int panelId() const override;

    public slots:
        void setNewSelection(const Selection&) override;
};
