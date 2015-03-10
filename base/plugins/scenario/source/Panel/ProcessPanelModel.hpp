#pragma once
#include <iscore/plugins/panel/PanelModelInterface.hpp>

class ProcessPanelModel : public iscore::PanelModelInterface
{
    public:
        ProcessPanelModel(QObject*);

    public slots:
        void setNewSelection(const Selection&) override;
};
