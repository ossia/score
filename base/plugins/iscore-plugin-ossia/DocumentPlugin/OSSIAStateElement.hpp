#pragma once
#include <QObject>
#include <memory>
class StateModel;
namespace OSSIA
{
    class State;
}
class OSSIAStateElement : public QObject
{
    public:
        OSSIAStateElement(
                std::shared_ptr<OSSIA::State> event,
                const StateModel* element,
                QObject* parent);

        std::shared_ptr<OSSIA::State> state() const;


    private:
        std::shared_ptr<OSSIA::State> m_state;
};
