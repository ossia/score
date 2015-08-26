#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>
namespace iscore { class DocumentModel; }
class GroupManager;
class Session;
class GroupPanelModel : public iscore::PanelModel
{
        Q_OBJECT
    public:
        explicit GroupPanelModel(iscore::DocumentModel* model);

        int panelId() const override;

        GroupManager* manager() const;
        Session* session() const;

    signals:
        void update();

    private:
        void scanPlugins();

        GroupManager* m_currentManager{};
        Session* m_currentSession{};
};
