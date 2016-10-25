#pragma once
#include <Engine/LocalTree/BaseProperty.hpp>
#include <iscore/model/ModelMetadata.hpp>

class MetadataNamePropertyWrapper
{
        iscore::ModelMetadata& metadata;
    public:
        ossia::net::node_base& node;

        MetadataNamePropertyWrapper(
                ossia::net::node_base& parent,
                iscore::ModelMetadata& arg_metadata,
                QObject* context
                ):
            metadata{arg_metadata},
            node{*parent.createChild(arg_metadata.getName().toStdString())}
        {
            /* // TODO do me with nano-signal-slot in device.hpp
            m_callbackIt =
                    node->addCallback(
                        [=] (const OSSIA::net::Node& node, const std::string& name, OSSIA::net::NodeChange t) {
                if(t == OSSIA::net::NodeChange::RENAMED)
                {
                    auto str = QString::fromStdString(node.getName());
                    if(str != metadata().name())
                        metadata().setName(str);
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
                      qDebug() << "ERROR (old/new)" << real_newName << newName;

                    if(real_newName != newName)
                    {
                        metadata.setName(QString::fromStdString(real_newName));
                    }
                }
            };

            QObject::connect(
                        &metadata, &iscore::ModelMetadata::NameChanged,
                        context, setNameFun,
                        Qt::QueuedConnection);

            setNameFun(metadata.getName());
        }

        ~MetadataNamePropertyWrapper()
        {
          auto par = node.getParent();
          if(par)
            par->removeChild(node);
        }
};
