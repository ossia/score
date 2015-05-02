#pragma once
#include <QObject>
#include "ProcessViewModelInterface.hpp"
class ProcessViewModelPanelProxy : public QObject
{
    public:
        using QObject::QObject;
        virtual ~ProcessViewModelPanelProxy() = default;

        // Can return the same view model, or a new one.
        virtual const ProcessViewModelInterface& viewModel() = 0;

};
