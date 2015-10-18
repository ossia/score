#pragma once

#include <iscore/command/Command.hpp>
#include <QApplication>

class ClientNameChangedCommand : public iscore::Command
{
        // QUndoCommand interface
    public:
        /*
        ClientNameChangedCommand(QString oldval, QString newval) :
            Command {QString(""),
                    QString(""),
                    QString("")
        },
        m_oldval {oldval},
        m_newval {newval}
        {

        }
*/
        /*
        virtual bool mergeWith(const Command* other) override
        {
            auto cmd = static_cast<const ClientNameChangedCommand*>(other);
            m_newval = cmd->m_newval;
            return true;
        }
        */

        void undo() const override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setClientName(m_oldval);
        }

        void redo() const override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setClientName(m_newval);
        }

    private:
        QString m_oldval;
        QString m_newval;
};
