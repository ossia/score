#pragma once
#include <QMetaType>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
class ProcessState
{
    public:
        ProcessState() = default;
        ProcessState(const ProcessState&) = default;
        ProcessState(ProcessState&&) = default;
        ProcessState& operator=(const ProcessState&) = default;
        ProcessState& operator=(ProcessState&&) = default;

        ProcessState(ProcessStateDataInterface* d):
            m_data{d}
        {

        }

        bool operator==(const ProcessState& p) const
        {
            return m_data == p.m_data;
        }

        bool operator<(const ProcessState& p) const
        {
            return false;
        }

    private:
        ProcessStateDataInterface* m_data{};
};

Q_DECLARE_METATYPE(ProcessState)
