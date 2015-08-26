#pragma once
#include <QMetaType>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>

// A wrapper for a dynamic state
class ProcessState
{
    public:
        ProcessState() = default;
        ProcessState(const ProcessState& other):
            m_data{other.m_data
                    ? other.m_data->clone()
                    : nullptr}
        {

        }

        ProcessState(ProcessState&&) = default;
        ProcessState& operator=(const ProcessState& other)
        {
            if(&other == this)
                return *this;

            delete m_data;
            m_data = other.m_data
                    ? other.m_data->clone()
                    : nullptr;
            return *this;
        }

        ProcessState& operator=(ProcessState&& other)
        {
            if(&other == this)
                return *this;

            delete m_data;
            m_data = other.m_data;
            other.m_data = nullptr;
            return *this;
        }

        explicit ProcessState(ProcessStateDataInterface* d):
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
