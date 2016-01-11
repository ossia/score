#pragma once

#include <iscore/plugins/panel/PanelModel.hpp>
namespace iscore
{
    class DocumentModel;
}
class LibraryPanelModel : public iscore::PanelModel
{
        Q_OBJECT
    public:
        LibraryPanelModel(QObject* parent);
        int panelId() const override;
};
