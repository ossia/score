#pragma once
#include <QObject>
class ProcessSharedModelInterface;
class ProcessStateDataInterface : public QObject
{
        Q_OBJECT
    public:
        ProcessStateDataInterface(const ProcessSharedModelInterface* model):
            m_model{model}
        {

        }

        virtual QString stateName() const = 0;

    signals:
        void stateChanged();

    protected:
        const ProcessSharedModelInterface* model() const
        { return m_model; }

    private:
        const ProcessSharedModelInterface* m_model{};
};
