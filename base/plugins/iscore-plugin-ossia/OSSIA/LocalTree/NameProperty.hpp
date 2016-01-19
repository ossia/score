#pragma once
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <Process/ModelMetadata.hpp>

class MetadataNamePropertyWrapper
{
        ModelMetadata& metadata;
        OSSIA::CallbackContainer<OSSIA::NodeChangeCallback>::iterator m_callbackIt;

    public:
        std::shared_ptr<OSSIA::Node> node;

        MetadataNamePropertyWrapper(
                OSSIA::Node& parent,
                ModelMetadata& arg_metadata,
                QObject* context
                ):
            metadata{arg_metadata}
        {
            node = *parent.emplace(
                       parent.children().end(),
                       arg_metadata.name().toStdString());

            m_callbackIt =
                    node->addCallback(
                        [=] (const OSSIA::Node& node, OSSIA::NodeChange t) {
                if(t == OSSIA::NodeChange::RENAMED)
                {
                    auto str = QString::fromStdString(node.getName());
                    if(str != metadata.name())
                        metadata.setName(str);
                }
            });

            auto setNameFun = [=] (const QString& newName_qstring) {
                auto newName = newName_qstring.toStdString();
                auto curName = node->getName();

                if(curName != newName)
                {
                    node->setName(newName);
                    auto real_newName = node->getName();

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
            node->removeCallback(m_callbackIt);
        }
};
