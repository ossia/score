#pragma once
#include <QVector>

class MoveEventFactoryInterface;

class MoveEventList
{
    public:
        enum Strategy{ MOVING, CREATION, EXTRA };

        /**
     * @brief getMoveEventFactory
     * @return
     * the factory with the highest priority for the specified strategy
     */
        MoveEventFactoryInterface* get(MoveEventList::Strategy strategy);

        /**
     * @brief registerMoveEventFactory
     * register a moveEvent with a unique priority (higher the better),
     * WARNING, if the same priority is already there, it will be overriden
     * @param factoryInterface
     * WARNING: this has to be of type MoveEventFactoryInterface unless it will crash
     */
        void inscribe(MoveEventFactoryInterface* factoryInterface);

    private:
        QVector<MoveEventFactoryInterface*> m_list;
};

class SingletonMoveEventList
{
    public:
        SingletonMoveEventList() = delete;
        static MoveEventList& instance();
};

