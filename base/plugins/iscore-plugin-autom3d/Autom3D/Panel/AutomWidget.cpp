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
    //auto spline = vtkSplineWidget::New();
    //spline->SetInteractor(iren);

    m_connections = vtkEventQtSlotConnect::New();

    // update coords as we move through the window
    m_connections->Connect(m_widget->GetRenderWindow()->GetInteractor(),
                           vtkCommand::MouseMoveEvent,
                           this,
                           SLOT(updateCoords(vtkObject*)));

    renwin->Delete();
}

AutomWidget::~AutomWidget()
{
    m_renderer->Delete();

    m_connections->Delete();
}


void AutomWidget::updateCoords(vtkObject* obj)
{
    // get interactor
    vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::SafeDownCast(obj);
    // get event position
    int event_pos[2];
    iren->GetEventPosition(event_pos);
    // update label
    QString str;
    str.sprintf("x=%d : y=%d", event_pos[0], event_pos[1]);
    qDebug() << str;
}
