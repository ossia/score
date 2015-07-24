#pragma once
#include <QObject>
class Process;
class ProcessStateDataInterface : public QObject
{
        Q_OBJECT
    public:
        ProcessStateDataInterface(const Process* model):
            m_model{model}
        {

        }

        virtual QString stateName() const = 0;

    signals:
        void stateChanged();

    protected:
        const Process* model() const
        { return m_model; }

    private:
        const Process* m_model{};
};
