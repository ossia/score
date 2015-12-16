#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace iscore {
class DocumentContext;
}
class SpaceProcess;
class AreaWidget;
class AreaTab : public QWidget
{
        Q_OBJECT
    public:
        AreaTab(
                const iscore::DocumentContext& ctx,
                const SpaceProcess &space,
                QWidget* parent);

    private slots:
        void updateDisplayedArea(int);
        void newArea();

    private:
        void rebuildList();

        const SpaceProcess &m_space;
        QListWidget* m_listWidget{};
        AreaWidget* m_areaWidget{};
};
