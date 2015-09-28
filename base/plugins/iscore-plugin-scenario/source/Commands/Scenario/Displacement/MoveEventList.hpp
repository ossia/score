#pragma once

#include <QMap>
#include <iscore/tools/NamedObject.hpp>
#include <QApplication>

class MoveEventFactoryInterface;
namespace iscore
{
class FactoryInterface;
}

class MoveEventList : public NamedObject
{
    Q_OBJECT
public:
    MoveEventList(QObject* parent)
        :NamedObject("MoveEventList", parent){};

    enum Strategy{ MOVING, CREATION, EXTRA };

    /**
     * @brief getMoveEventFactory
     * @return
     * the factory with the highest priority for the specified strategy
     */
    MoveEventFactoryInterface* getMoveEventFactory(MoveEventList::Strategy strategy);

    /**
     * @brief registerMoveEventFactory
     * register a moveEvent with a unique priority (higher the better),
     * WARNING, if the same priority is already there, it will be overriden
     * @param factoryInterface
     * WARNING: this has to be of type MoveEventFactoryInterface unless it will crash
     */
    void registerMoveEventFactory(iscore::FactoryInterface* factoryInterface);

    static MoveEventFactoryInterface* getFactory(MoveEventList::Strategy strategy)
    {

        return qApp
                ->findChild<MoveEventList*> ("MoveEventList")
                ->getMoveEventFactory(strategy);
    }

private:
    QVector<MoveEventFactoryInterface*> m_moveEventFactories;
};



