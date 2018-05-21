#pragma once
#include <ossia/dataflow/execution_state.hpp>
#include <wobjectdefs.h>
#include <ossia/detail/hash_map.hpp>

#include <QtQml/QJSEngine>
#include <score/tools/std/StringHash.hpp>

namespace JS
{
class ExecStateWrapper : public QObject
{
  W_OBJECT(ExecStateWrapper)
public:
  ExecStateWrapper(const ossia::execution_state& state, QObject* parent)
      : QObject{parent}, devices{state}
  {
  }
  ~ExecStateWrapper() override;

public:
  QVariant read(const QString& address); W_SLOT(read);
  void write(const QString& address, const QVariant& value); W_SLOT(write);

private:
  const ossia::execution_state& devices;

  ossia::net::parameter_base* find_address(const QString&);
  ossia::fast_hash_map<QString, ossia::net::parameter_base*> m_address_cache;
};
}
