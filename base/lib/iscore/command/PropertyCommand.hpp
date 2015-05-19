#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
namespace iscore
{
    class PropertyCommand : public SerializableCommand
    {
        public:
            PropertyCommand(ObjectPath&& path, QString property);

            void undo();
            void redo();

        protected:
            void serializeImpl(QDataStream &) const;
            void deserializeImpl(QDataStream &);

        private:
            ObjectPath m_path;
            QString m_property;
            QVariant m_old, m_new;
    };
}
