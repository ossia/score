#pragma once
#include <QWidget>

class vtkRenderer;
class vtkEventQtSlotConnect;
class vtkObject;
class vtkCommand;
class QVTKWidget;

class AutomWidget : public QWidget
{
        Q_OBJECT
    public:
        AutomWidget();
        ~AutomWidget();

    public slots:
        void updateCoords(vtkObject*);

    protected:
        QVTKWidget* m_widget{};
        vtkRenderer* m_renderer{};
        vtkEventQtSlotConnect* m_connections{};
};
