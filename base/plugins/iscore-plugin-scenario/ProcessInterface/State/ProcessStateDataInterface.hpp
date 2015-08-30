#pragma once
#include <QObject>
#include <State/DynamicStateDataInterface.hpp>
#include <ProcessInterface/Process.hpp>

class ProcessStateDataInterface : public DynamicStateDataInterface
{
    public:
        ProcessStateDataInterface(const Process& model, QObject* parent):
            DynamicStateDataInterface{parent},
            m_model{model}
        {
        }

    protected:
        const Process& model() const
        { return m_model; }

    private:
        const Process& m_model;
};
