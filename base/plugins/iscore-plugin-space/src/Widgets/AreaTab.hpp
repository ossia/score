#pragma once
#include <QtWidgets>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace iscore {
struct DocumentContext;
}
namespace Space { class ProcessModel; }
class AreaWidget;
class AreaTab : public QWidget
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

        const Space::ProcessModel &m_space;
        QListWidget* m_listWidget{};
        AreaWidget* m_areaWidget{};
};
