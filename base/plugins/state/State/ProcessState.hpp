#pragma once
#include <QMetaType>

class ProcessStateDataInterface : public QObject
{

};

class ProcessState
{
    public:
        ProcessState() = default;
        ProcessState(const ProcessState&) = default;
        ProcessState(ProcessState&&) = default;
        ProcessState& operator=(const ProcessState&) = default;
        ProcessState& operator=(ProcessState&&) = default;

        bool operator==(const ProcessState& p) const
        {
            return false;
        }

        bool operator<(const ProcessState& p) const
        {
            return false;
        }

    private:
        ProcessStateDataInterface* m_data{};
};

Q_DECLARE_METATYPE(ProcessState)
