#pragma once
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QtQml/QJSEngine>
#include <score/tools/std/StringHash.hpp>

namespace JS
{
class ExecStateWrapper : public QObject
{
  Q_OBJECT
public:
  ExecStateWrapper(QJSEngine& engine, const ossia::execution_state& state, QObject* parent)
      : QObject{parent}, m_engine{engine}, devices{state}
  {
  }
  ~ExecStateWrapper() override;

public Q_SLOTS:
  QVariant read(const QString& address);
  void write(const QString& address, const QVariant& value);

private:
  QJSEngine& m_engine;
  const ossia::execution_state& devices;

  ossia::net::parameter_base* find_address(const QString&);
  ossia::fast_hash_map<QString, ossia::net::parameter_base*> m_address_cache;
};
}
