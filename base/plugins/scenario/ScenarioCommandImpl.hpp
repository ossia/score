#ifndef SCENARIOCOMMANDIMPL_H
#define SCENARIOCOMMANDIMPL_H

#include <QString>
#include <QPointF>
#include <core/presenter/command/Command.hpp>

class ScenarioCommandImpl : public iscore::Command
{
    public:
        ScenarioCommandImpl(QPointF pos);

        virtual QByteArray serialize() override;

        void deserialize(QByteArray arr) override;

        virtual void undo() override;

        virtual void redo() override;

    private:
        QString m_parentName;
        QPointF m_position;
};

#endif // SCENARIOCOMMANDIMPL_H
