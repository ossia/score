#pragma once
#include <QObject>
#include <State/DynamicStateDataInterface.hpp>
#include <ProcessInterface/Process.hpp>

class ProcessStateDataInterface : public DynamicStateDataInterface
{
    public:
        ProcessStateDataInterface(Process& model, QObject* parent):
            DynamicStateDataInterface{parent},
            m_model{model}
        {
        }

    protected:
        Process& model() const
        { return m_model; }

    private:
        Process& m_model;
};
