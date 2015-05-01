#pragma once

#include <iscore/plugins/panel/PanelModelInterface.hpp>
namespace iscore
{
    class DocumentModel;
}
/**
 * @brief The InspectorPanelModel class
 *
 * @todo Inspector Model : keep the currently identified object. (ObjectPath)
 */
class InspectorPanelModel : public iscore::PanelModelInterface
{
        Q_OBJECT
    public:
        InspectorPanelModel(iscore::DocumentModel* parent);
        int panelId() const override;

    signals:
        void selectionChanged(const Selection& s);

    public slots:
        virtual void setNewSelection(const Selection& s) override;

};
