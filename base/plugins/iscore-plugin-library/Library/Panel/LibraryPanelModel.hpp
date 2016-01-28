#pragma once

#include <iscore/plugins/panel/PanelModel.hpp>
namespace iscore
{
    class DocumentModel;
}

namespace Library
{
class LibraryPanelModel : public iscore::PanelModel
{
        Q_OBJECT
    public:
        LibraryPanelModel(QObject* parent);
        int panelId() const override;
};
}
