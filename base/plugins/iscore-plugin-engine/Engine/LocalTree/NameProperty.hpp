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
      QObject* context)
      : metadata{arg_metadata}
      , node{*parent.create_child(arg_metadata.getName().toStdString())}
  {
    /* // TODO do me with nano-signal-slot in device.hpp
    m_callbackIt =
            node->addCallback(
                [=] (const OSSIA::net::Node& node, const std::string& name,
    OSSIA::net::NodeChange t) {
        if(t == OSSIA::net::NodeChange::RENAMED)
        {
            auto str = QString::fromStdString(node.getName());
            if(str != metadata().name())
                metadata().setName(str);
        }
    });
    */

    auto setNameFun = [=](const QString& newName_qstring) {
      auto newName = newName_qstring.toStdString();
      auto curName = node.get_name();

      if (curName != newName)
      {
        node.set_name(newName);
        auto real_newName = node.get_name();
        if (real_newName != newName)
          qDebug() << "ERROR (old/new)" << real_newName << newName;

        if (real_newName != newName)
        {
          metadata.setName(QString::fromStdString(real_newName));
        }
      }
    };

    QObject::connect(
        &metadata, &iscore::ModelMetadata::NameChanged, context, setNameFun);

    setNameFun(metadata.getName());
  }

  ~MetadataNamePropertyWrapper()
  {
    auto par = node.get_parent();
    if (par)
      par->remove_child(node);
  }
};
