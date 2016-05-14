#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/Process.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class ProcessWidgetArea final : public Inspector::InspectorSectionWidget
{
        Q_OBJECT
    public:
        template<typename... Args>
        ProcessWidgetArea(const Process::ProcessModel& proc, CommandDispatcher<>& disp, Args&&... args):
            InspectorSectionWidget{std::forward<Args>(args)...},
            m_proc{proc},
            m_disp{disp}
        {
            setAcceptDrops(true);

            connect(this, &ProcessWidgetArea::sig_performSwap,
                    this, &ProcessWidgetArea::performSwap,
                    Qt::QueuedConnection);
        }

    private:
        void mousePressEvent(QMouseEvent * event) override;
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

    signals:
        void sig_performSwap(Path<Scenario::ConstraintModel> cst,
                             const Id<Process::ProcessModel>& id1,
                             const Id<Process::ProcessModel>& id2);

    private slots:
        void performSwap(Path<Scenario::ConstraintModel> cst,
                         const Id<Process::ProcessModel>& id1,
                         const Id<Process::ProcessModel>& id2);

    private:
        const Process::ProcessModel& m_proc;
        CommandDispatcher<>& m_disp;
};
}
