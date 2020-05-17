#pragma once
#include <score/model/ModelMetadata.hpp>

#include <ossia/network/base/node.hpp>

#include <QDebug>

#include <LocalTree/BaseProperty.hpp>
namespace LocalTree
{

class MetadataNamePropertyWrapper
{
  score::ModelMetadata& metadata;

public:
  ossia::net::node_base& node;

  MetadataNamePropertyWrapper(
      ossia::net::node_base& parent,
      score::ModelMetadata& arg_metadata,
      QObject* context)
      : metadata{arg_metadata}, node{*parent.create_child(arg_metadata.getName().toStdString())}
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
      const auto newName = newName_qstring.toStdString();
      const auto curName = node.get_name();

      if (curName != newName)
      {
        node.set_name(newName);
        auto real_newName = node.get_name();
        if (real_newName != newName)
        {
          const auto& x = QString::fromStdString(real_newName);
          qDebug() << "ERROR (real_newName/newName)" << x << newName_qstring;
          metadata.setName(x);
        }
      }
    };

    QObject::connect(&metadata, &score::ModelMetadata::NameChanged, context, setNameFun);

    setNameFun(metadata.getName());
  }

  ~MetadataNamePropertyWrapper()
  {
    auto par = node.get_parent();
    if (par)
      par->remove_child(node);
  }
};

}
