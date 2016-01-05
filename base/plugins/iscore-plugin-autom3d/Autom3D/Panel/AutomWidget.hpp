#pragma once
#include <QWidget>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
class vtkRenderer;
class vtkEventQtSlotConnect;
class vtkObject;
class vtkCommand;
class vtkSplineWidget2;
class QVTKWidget;
namespace iscore
{
class CommandStackFacade;
}
namespace Autom3D
{
class ProcessModel;
class AutomWidget final : public QWidget
{
        Q_OBJECT
    public:
        AutomWidget(
                const ProcessModel& proc,
                iscore::CommandStackFacade& stck);
        ~AutomWidget();

    public slots:
        void press(vtkObject*);
        void release(vtkObject*);

    private:
        void on_handlesChanged();
        CommandDispatcher<> m_disp;
        const ProcessModel& m_proc;
        QVTKWidget* m_widget{};
        vtkRenderer* m_renderer{};
        vtkEventQtSlotConnect* m_connections{};

        vtkSplineWidget2* m_spline{};
};
}
