#pragma once
#include <QObject>
class ProcessModel;
class ProcessStateDataInterface : public QObject
{
        Q_OBJECT
    public:
        ProcessStateDataInterface(const ProcessModel* model):
            m_model{model}
        {

        }

        virtual QString stateName() const = 0;

    signals:
        void stateChanged();

    protected:
        const ProcessModel* model() const
        { return m_model; }

    private:
        const ProcessModel* m_model{};
};
