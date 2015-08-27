#pragma once
#include <QObject>
#include <State/DynamicStateDataInterface.hpp>

class Process;

class ProcessStateDataInterface : public DynamicStateDataInterface
{
    public:
        ProcessStateDataInterface(const Process* model):
            DynamicStateDataInterface{},
            m_model{model}
        {

        }

    protected:
        const Process* model() const
        { return m_model; }

    private:
        const Process* m_model{};
};
