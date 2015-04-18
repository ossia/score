#pragma once
#include <iscore/plugins/panel/PanelModelInterface.hpp>
namespace iscore { class DocumentModel; }
class GroupManager;
class Session;
class GroupPanelModel : public iscore::PanelModelInterface
{
        Q_OBJECT
    public:
        GroupPanelModel(iscore::DocumentModel* model);

        auto manager() const
        { return m_currentManager; }
        auto session() const
        { return m_currentSession; }

    signals:
        void update();

    private:
        void scanPlugins();

        GroupManager* m_currentManager{};
        Session* m_currentSession{};
};
