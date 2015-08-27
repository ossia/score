#pragma once
#include <QMetaType>
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>

/**
 * @brief The ProcessState class
 *
 * A wrapper for dynamic states defined in plug-ins. It is to be used
 * as a data member in a StateNode.
 */
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
