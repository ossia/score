#pragma once
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <Process/ModelMetadata.hpp>

class MetadataNamePropertyWrapper
{
        ModelMetadata& metadata;
    public:
        OSSIA::net::Node& node;

        MetadataNamePropertyWrapper(
                OSSIA::net::Node& parent,
                ModelMetadata& arg_metadata,
                QObject* context
                ):
            metadata{arg_metadata},
            node{*parent.createChild(arg_metadata.name().toStdString())}
        {
            /* // TODO do me with nano-signal-slot in device.hpp
            m_callbackIt =
                    node->addCallback(
                        [=] (const OSSIA::net::Node& node, const std::string& name, OSSIA::net::NodeChange t) {
                if(t == OSSIA::net::NodeChange::RENAMED)
                {
                    auto str = QString::fromStdString(node.getName());
                    if(str != metadata.name())
                        metadata.setName(str);
                }
            });
            */

            auto setNameFun = [=] (const QString& newName_qstring) {
                auto newName = newName_qstring.toStdString();
                auto curName = node.getName();

                if(curName != newName)
                {
                    node.setName(newName);
                    auto real_newName = node.getName();

                    if(real_newName != newName)
                    {
                        metadata.setName(QString::fromStdString(real_newName));
                    }
                }
            };

            QObject::connect(
                        &metadata, &ModelMetadata::nameChanged,
                        context, setNameFun,
                        Qt::QueuedConnection);

            setNameFun(metadata.name());
        }

        ~MetadataNamePropertyWrapper()
        {
            //node->removeCallback(m_callbackIt);
            node.getParent()->removeChild(node);
        }
};
