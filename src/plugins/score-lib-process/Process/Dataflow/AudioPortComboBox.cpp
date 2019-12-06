#include "AudioPortComboBox.hpp"
#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::AudioPortComboBox)
namespace Process
{
AudioPortComboBox::AudioPortComboBox(
    const State::Address& rootAddress,
    const Device::Node& node,
    QWidget* parent)
    : QComboBox{parent}
    , m_root{rootAddress}
{
  m_address.address = rootAddress;
  if(!node.children().empty())
  {
      auto& first_child = node.children().front().target<Device::AddressSettings>()->name;
      m_address.address.path.push_back(first_child);
  }

  connect(this, qOverload<const QString&>(&QComboBox::currentIndexChanged), [=] (const QString& str) {
      if(str == "None")
      {
          if(m_address.address != m_root)
          {
              m_address.address = m_root;
              addressChanged(m_address);
          }
      }
      else
      {
          auto addr = m_root;
          addr.path.push_back(str);
          if(m_address.address != addr)
          {
            m_address.address = std::move(addr);
            addressChanged(m_address);
          }
      }
  });

  m_child.push_back("None");
  for(auto& child : node)
  {
    const QString& name = child.target<Device::AddressSettings>()->name;
    addItem(name);
    m_child.push_back(name);
  }
}

void AudioPortComboBox::setAddress(const State::Address& addr)
{
  m_address.address = m_root;

  if(!addr.path.empty())
  {
      auto& name = addr.path.back();
      for(int i = 0; i < m_child.size(); i++)
      {
          if(m_child[i] == name)
          {
              m_address.address.path.push_back(name);
              setCurrentIndex(i);
              return;
          }
      }
  }

  setCurrentIndex(0);
}

const Device::FullAddressSettings&
AudioPortComboBox::address() const
{
  return m_address;
}

QString AudioPortComboBox::addressString() const
{
  return m_address.address.toString();
}
}
