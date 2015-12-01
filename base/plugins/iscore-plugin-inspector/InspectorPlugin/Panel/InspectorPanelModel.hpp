#pragma once

#include <iscore/plugins/panel/PanelModel.hpp>

#include <iscore/selection/Selection.hpp>

namespace iscore
{
    class DocumentModel;
}
/**
 * @brief The InspectorPanelModel class
 *
 * @todo Inspector Model : keep the currently identified object. (ObjectPath)
 */
class InspectorPanelModel : public iscore::PanelModel
{
        Q_OBJECT
    public:
        explicit InspectorPanelModel(QObject* parent);
        int panelId() const override;

    signals:
        void selectionChanged(const Selection& s);

    public slots:
        void setNewSelection(const Selection& s) override;

};
