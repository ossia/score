#include "ProcessWidgetArea.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Commands/Constraint/SetProcessPosition.hpp>
#include <Process/Process.hpp>

#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

namespace Scenario
{

// MOVEME
template<typename T>
auto id(const Path<T>& path)
{
    ISCORE_ASSERT(path.valid());
    ISCORE_ASSERT(path.unsafePath().vec().back().id());

    return Id<T>(*path.unsafePath().vec().back().id());
}

void ProcessWidgetArea::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {

        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        mimeData->setData("application/x-iscore-processdrag",
                          marshall<DataStream>(make_path(m_proc)));
        drag->setMimeData(mimeData);
        QLabel label{m_proc.metadata.name()};
        drag->setPixmap(label.grab());
        drag->setHotSpot(label.rect().center());

        Qt::DropAction dropAction = drag->exec();
        // TODO drop must only work in same application !!!
        // TODO drop must only work in same time constraint !!!
        // or else drop it in another process but what to do with the rack / slots ?
    }
}

void ProcessWidgetArea::dragEnterEvent(QDragEnterEvent* event)
{
    if(!event->mimeData()->hasFormat("application/x-iscore-processdrag"))
        return;

    auto path = unmarshall<Path<Process::ProcessModel>>(event->mimeData()->data("application/x-iscore-processdrag"));
    auto res = path.try_find();
    if(!res)
        return;

    if(res->parent() != m_proc.parent())
        return;

    event->acceptProposedAction();
}

void ProcessWidgetArea::dropEvent(QDropEvent* event)
{
    // Get the process
    auto path = unmarshall<Path<Process::ProcessModel>>(event->mimeData()->data("application/x-iscore-processdrag"));

    emit sig_performSwap(
                *safe_cast<Scenario::ConstraintModel*>(m_proc.parent()),
                m_proc.id(),
                id(path));
    // Accept
    event->acceptProposedAction();
}

void ProcessWidgetArea::performSwap(Path<Scenario::ConstraintModel> cst, const Id<Process::ProcessModel>& id1, const Id<Process::ProcessModel>& id2)
{
    // Create a command to swap both processes
    m_disp.submitCommand(
                new Command::SetProcessPosition{
                    std::move(cst), id1, id2
                });

}


}
