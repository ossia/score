#pragma once
#include <QString>
#include <vector>

namespace iscore
{
    class SerializableCommand;

    class PanelFactory;
    class PluginControlInterface;
    /**
     * Utility namespace for access to the application presenter features.
     */
    namespace IPresenter
    {
        /**
         * @brief instantiateUndoCommand Is used to generate a Command from its serialized data.
         * @param parent_name The name of the object able to generate the command. Must be a CustomCommand.
         * @param name The name of the command to generate.
         * @param data The data of the command.
         *
         * Ownership of the command is transferred to the caller, and he must delete it.
         *
         * This will throw a MissingCommandException if not found.
         */
        iscore::SerializableCommand*
        instantiateUndoCommand(const QString& parent_name,
                               const QString& name,
                               const QByteArray& data);

        QList<iscore::PanelFactory*> panelFactories();
        const std::vector<iscore::PluginControlInterface*>& pluginControls();
    }
}
