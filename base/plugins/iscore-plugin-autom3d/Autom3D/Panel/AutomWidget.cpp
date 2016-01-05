#include "AutomWidget.hpp"

#include <QMenu>

#include "vtkRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkCommand.h"
#include "vtkEventQtSlotConnect.h"
#include "vtkConeSource.h"
#include "vtkSphereSource.h"
#include "vtkPolyDataMapper.h"
#include "vtkActor.h"
#include "vtkInteractorStyle.h"
#include "vtkTDxInteractorStyleCamera.h"
#include "vtkTDxInteractorStyleSettings.h"
#include "QVTKInteractor.h"
#include <QVBoxLayout>
#include <QVTKWidget.h>
#include <vtkSplineWidget.h>
#include <vtkSplineWidget2.h>
#include <vtkCubeAxesActor.h>
#include <vtkSplineRepresentation.h>
#include <vtkParametricSpline.h>
#include <vtkSmartPointer.h>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <Autom3D/Commands/ChangeAddress.hpp>
#include <Autom3D/Autom3DModel.hpp>
namespace Autom3D
{
AutomWidget::AutomWidget(
        const ProcessModel& proc,
        iscore::CommandStackFacade& stck):
    m_disp{stck},
    m_proc{proc}
{
    m_widget = new QVTKWidget;
    auto lay = new QVBoxLayout;
    this->setLayout(lay);
    lay->addWidget(m_widget);

    // create a window to make it stereo capable and give it to QVTKWidget
    vtkRenderWindow* renwin = vtkRenderWindow::New();

    m_widget->SetRenderWindow(renwin);

    QVTKInteractor *iren = m_widget->GetInteractor();

    // add a renderer
    m_renderer = vtkRenderer::New();
    m_widget->GetRenderWindow()->AddRenderer(m_renderer);

    // put cone in one window
    m_spline = vtkSplineWidget2::New();
    m_spline->SetInteractor(iren);
    m_spline->On();

    auto cubeAxesActor = vtkCubeAxesActor::New();

    cubeAxesActor->SetCamera(m_renderer->GetActiveCamera());
    m_renderer->AddActor(cubeAxesActor);
    m_connections = vtkEventQtSlotConnect::New();

    cubeAxesActor->XAxisLabelVisibilityOff();
    cubeAxesActor->YAxisLabelVisibilityOff();
    cubeAxesActor->ZAxisLabelVisibilityOff();

    cubeAxesActor->XAxisTickVisibilityOff();
    cubeAxesActor->YAxisTickVisibilityOff();
    cubeAxesActor->ZAxisTickVisibilityOff();

    cubeAxesActor->XAxisMinorTickVisibilityOff();
    cubeAxesActor->YAxisMinorTickVisibilityOff();
    cubeAxesActor->ZAxisMinorTickVisibilityOff();
    // update coords as we move through the window

    m_connections->Connect(m_spline,
                           vtkCommand::StartInteractionEvent,
                           this,
                           SLOT(press(vtkObject*)));

    m_connections->Connect(m_spline,
                           vtkCommand::EndInteractionEvent,
                           this,
                           SLOT(release(vtkObject*)));


    renwin->Delete();

    con(m_proc, &ProcessModel::handlesChanged,
        this, &AutomWidget::on_handlesChanged);
    on_handlesChanged();
}

AutomWidget::~AutomWidget()
{
    m_renderer->Delete();
    m_connections->Delete();
}


void AutomWidget::press(vtkObject* obj)
{
}

void AutomWidget::release(vtkObject* obj)
{
    auto spl = vtkSplineRepresentation::SafeDownCast(m_spline->GetRepresentation())->GetParametricSpline();
    auto& pts = *spl->GetPoints();

    std::vector<Point> handles;
    int n = pts.GetNumberOfPoints();
    handles.reserve(n);
    for(int i = 0; i < n; i++)
    {
        double pt[3];
        pts.GetPoint(i, pt);
        handles.emplace_back(pt[0], pt[1], pt[2]);
    }

    auto cmd = new UpdateSpline{m_proc, std::move(handles)};
    m_disp.submitCommand(cmd);
}

void AutomWidget::on_handlesChanged()
{
    auto rep = vtkSplineRepresentation::SafeDownCast(m_spline->GetRepresentation());
    auto spl = rep->GetParametricSpline();

    spl->SetNumberOfPoints(0);

    auto newPoints = vtkPoints::New();
    for(const Point& pt : m_proc.handles())
    {
        newPoints->InsertNextPoint(pt.x(), pt.y(), pt.z());
    }
    spl->SetNumberOfPoints(newPoints->GetNumberOfPoints());
    rep->InitializeHandles(newPoints);
    m_widget->update();

}
}
