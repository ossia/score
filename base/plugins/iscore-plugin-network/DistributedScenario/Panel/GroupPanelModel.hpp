#pragma once
#include <iscore/plugins/panel/PanelModel.hpp>
namespace iscore {
class DocumentModel;
struct DocumentContext;
}

namespace Network
{
class GroupManager;
class Session;
class GroupPanelModel : public iscore::PanelModel
{
        Q_OBJECT
    public:
        explicit GroupPanelModel(
                const iscore::DocumentContext& ctx,
                QObject* parent);

        int panelId() const override;

        GroupManager* manager() const;
        Session* session() const;

    signals:
        void update();

    private:
        void scanPlugins(const iscore::DocumentContext& ctx);

        GroupManager* m_currentManager{};
        Session* m_currentSession{};
};
}
