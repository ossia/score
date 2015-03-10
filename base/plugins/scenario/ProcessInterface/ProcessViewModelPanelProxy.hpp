#pragma once
#include <QObject>
#include "ProcessViewModelInterface.hpp"
class ProcessViewModelPanelProxy : public QObject
{
    public:
        ProcessViewModelPanelProxy(ProcessViewModelInterface* pvm):
            QObject{pvm}
        {

        }

        virtual ~ProcessViewModelPanelProxy() = default;

        // Can return the same view model, or a new one.
        virtual ProcessViewModelInterface* viewModel() = 0;

};
