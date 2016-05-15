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
#include <QLabel>
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

        drag->exec();
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

    if(res == &m_proc)
        return;

    if(res->parent() != m_proc.parent())
        return;

    event->acceptProposedAction();
}

void ProcessWidgetArea::dragMoveEvent(QDragMoveEvent* event)
{
    // TODO : show a bar to indicate
    // graphically where the drop will occur.
}

void ProcessWidgetArea::dropEvent(QDropEvent* event)
{
    // Get the process
    auto path = unmarshall<Path<Process::ProcessModel>>(
                    event->mimeData()->data("application/x-iscore-processdrag"));

    auto& cst = *safe_cast<Scenario::ConstraintModel*>(m_proc.parent());
    // Position
    auto center = rect().center().y();
    auto y = event->pos().y();
    if(y < center)
    {
        // Drop before this
        emit sig_performSwap(cst, m_proc.id(), id(path));
    }
    else
    {
        // Drop after this
        auto next_proc_it = cst.processes.find(m_proc.id());
        std::advance(next_proc_it, 1);
        if(next_proc_it != cst.processes.end())
        {
            // Drop before next process
            emit sig_performSwap(cst, next_proc_it->id(), id(path));
        }
        else
        {
            // Drop at end
            emit sig_putAtEnd(cst, id(path));
        }
    }
    // Accept
    event->acceptProposedAction();
}

void ProcessWidgetArea::performSwap(
        Path<Scenario::ConstraintModel> cst,
        const Id<Process::ProcessModel>& id1,
        const Id<Process::ProcessModel>& id2)
{
    // Create a command to swap both processes
    m_disp.submitCommand(
                new Command::PutProcessBefore{
                    std::move(cst), id1, id2
                });

}

void ProcessWidgetArea::putAtEnd(
        Path<ConstraintModel> cst,
        const Id<Process::ProcessModel>& id1)
{
    m_disp.submitCommand(
                new Command::PutProcessToEnd{
                    std::move(cst), id1
                });
}


}
