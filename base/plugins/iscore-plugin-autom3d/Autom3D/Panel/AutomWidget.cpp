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
AutomWidget::AutomWidget()
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
    auto spline = vtkSplineWidget2::New();
    spline->SetInteractor(iren);
    spline->On();

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

    m_connections->Connect(spline,
                           vtkCommand::StartInteractionEvent,
                           this,
                           SLOT(press(vtkObject*)));

    m_connections->Connect(spline,
                           vtkCommand::EndInteractionEvent,
                           this,
                           SLOT(release(vtkObject*)));


    renwin->Delete();
}

AutomWidget::~AutomWidget()
{
    m_renderer->Delete();

    m_connections->Delete();
}


void AutomWidget::press(vtkObject* obj)
{
    // get interactor
    vtkSplineWidget2* spline = vtkSplineWidget2::SafeDownCast(obj);
    auto iren = spline->GetInteractor();

    // get event position
    int event_pos[2];
    iren->GetEventPosition(event_pos);
}

void AutomWidget::release(vtkObject* obj)
{
    // get interactor
    vtkSplineWidget2* spline = vtkSplineWidget2::SafeDownCast(obj);
    auto iren = spline->GetInteractor();
    // get event position
    int event_pos[2];
    iren->GetEventPosition(event_pos);
    spline->GetRepresentation();

}
