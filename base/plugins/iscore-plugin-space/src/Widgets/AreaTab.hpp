#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <nano_observer.hpp>

namespace iscore {
struct DocumentContext;
}
namespace Space
{
class ProcessModel;
class AreaWidget;
class AreaModel;
class AreaTab :
        public QWidget,
        public Nano::Observer
{
        Q_OBJECT
    public:
        AreaTab(
                const iscore::DocumentContext& ctx,
                const Space::ProcessModel &space,
                QWidget* parent);

    private slots:
        void updateDisplayedArea(int);
        void newArea();

    private:
        void rebuildList();
        void on_areaAdded(const AreaModel&);
        void on_areaRemoved(const AreaModel&);

        const Space::ProcessModel &m_space;
        QListWidget* m_listWidget{};
        AreaWidget* m_areaWidget{};
};
}
