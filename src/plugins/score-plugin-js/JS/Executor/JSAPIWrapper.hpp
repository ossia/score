#pragma once
#include <score/tools/std/StringHash.hpp>

#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/network/common/path.hpp>

#include <QObject>

#include <verdigris>
namespace ossia
{
struct execution_state;
}
namespace JS
{
class ExecStateWrapper : public QObject
{
  W_OBJECT(ExecStateWrapper)
public:
  ExecStateWrapper(ossia::execution_state& state, QObject* parent)
      : QObject{parent}, devices{state}
  {
  }
  ~ExecStateWrapper() override;

  QVariant read(const QString& address);
  W_SLOT(read);
  void write(const QString& address, const QVariant& value);
  W_SLOT(write);
  void exec(const QString& code) W_SIGNAL(exec, code);

private:
  ossia::execution_state& devices;

  const ossia::destination_t& find_address(const QString&);
  ossia::fast_hash_map<QString, ossia::destination_t> m_address_cache;
  // TODO share cash across
};
}
