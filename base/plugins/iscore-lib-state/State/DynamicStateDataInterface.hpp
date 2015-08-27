#pragma once
#include <QObject>

/**
 * @brief The DynamicStateDataInterface class
 *
 * An abstract class that is to be subclassed to provide custom states.
 */
class DynamicStateDataInterface : public QObject
{
        Q_OBJECT
    public:
        explicit DynamicStateDataInterface(QObject* parent):
            QObject{parent}
        {

        }

        virtual QString stateName() const = 0;
        virtual DynamicStateDataInterface* clone(QObject* parent) const = 0;

    signals:
        void stateChanged();
};
