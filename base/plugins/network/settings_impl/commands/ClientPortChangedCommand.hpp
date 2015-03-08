#pragma once

#include <iscore/command/Command.hpp>
#include <QApplication>

class ClientPortChangedCommand : public iscore::Command
{
        // QUndoCommand interface
    public:
        ClientPortChangedCommand(int oldval, int newval) :
            Command {QString(""),
                    QString(""),
                    QString("")
        },
        m_oldval {oldval},
        m_newval {newval}
        {

        }

        virtual bool mergeWith(const Command* other) override
        {
            auto cmd = static_cast<const ClientPortChangedCommand*>(other);
            m_newval = cmd->m_newval;
            return true;
        }

        virtual void undo() override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setClientPort(m_oldval);
        }

        virtual void redo() override
        {
            auto target = qApp->findChild<NetworkSettingsModel*> ("NetworkSettingsModel");
            target->setClientPort(m_newval);
        }

    private:
        int m_oldval;
        int m_newval;
};
