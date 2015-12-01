#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class SpaceProcess;
class AreaWidget;
class AreaTab : public QWidget
{
        Q_OBJECT
    public:
        AreaTab(iscore::CommandStackFacade &stack, const SpaceProcess &space, QWidget* parent);

    private slots:
        void updateDisplayedArea(int);
        void newArea();

    private:
        void rebuildList();

        const SpaceProcess &m_space;
        QListWidget* m_listWidget{};
        AreaWidget* m_areaWidget{};
};
