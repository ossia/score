#pragma once

#include <QMap>
#include <Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <QApplication>

class MoveEventList : public NamedObject
{
    Q_OBJECT
public:
    MoveEventList(QObject* parent)
        :NamedObject("MoveEventList", parent){};


    /**
     * @brief getMoveEventFactory
     * @return
     * the factory with the highest priority
     */
    MoveEventFactoryInterface* getMoveEventFactory();

    /**
     * @brief registerMoveEventFactory
     * register a moveEvent with a unique priority (higher the better),
     * WARNING, if the same priority is already there, it will be overriden
     * @param factoryInterface
     * WARNING: this has to be of type MoveEventFactoryInterface unless it will crash
     */
    void registerMoveEventFactory(iscore::FactoryInterface* factoryInterface);

    static MoveEventFactoryInterface* getFactory()
    {
        return qApp
                ->findChild<MoveEventList*> ("MoveEventList")
                ->getMoveEventFactory();
    }

private:
    QMap<int,MoveEventFactoryInterface*> m_moveEventFactories;
};



