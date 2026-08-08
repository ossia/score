#pragma once
/**
 * Hiding a factory from this build, for a scope.
 *
 * The session tests run both peers in one process, sharing one set of
 * factories, so the case that matters most on a terminal cannot be written: a
 * peer that *cannot construct* what the other side can. A browser has no evdev,
 * no MIDI, no camera; it carries their settings without ever decoding them, and
 * hands them back when it asks for a device to be made.
 *
 * Every device bug in that story was this asymmetry, and the suite caught none
 * of them -- they were found by hand against a real wasm client, and one of them
 * needed the wire hand-written byte by byte to reproduce at all. A test that
 * passes with both sides symmetric proves the case that was never in doubt.
 *
 * So: take the factory out of the application's list, run the half of the
 * exchange that must happen without it, and put it back. What the code does
 * while it is gone is what a terminal does all the time.
 *
 *     QByteArray wire;
 *     {
 *       score::test::absent_factory<Device::ProtocolFactoryList> hidden{ctx, key};
 *       REQUIRE(hidden.was_present());   // or the test proves nothing
 *       DataStream::Serializer s{&wire};
 *       s.readFrom(settings);            // as the machine without it writes
 *     }
 *     DataStream::Deserializer d{wire};
 *     d.writeTo(received);               // as the machine with it reads
 */
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/UuidKey.hpp>

#include <memory>
#include <utility>

namespace score::test
{
template <typename FactoryList>
class absent_factory
{
public:
  using key_type = typename FactoryList::key_type;

  absent_factory(const score::ApplicationContext& ctx, const key_type& key)
      // The list is const through the context because nothing in score changes
      // it after load. A test that puts back what it took is the exception.
      : m_list{const_cast<FactoryList&>(ctx.interfaces<FactoryList>())}
      , m_key{key.impl()}
  {
    if(auto it = m_list.map.find(m_key); it != m_list.map.end())
    {
      m_held = std::move(it->second);
      m_list.map.erase(it);
    }
  }

  ~absent_factory()
  {
    if(m_held)
      m_list.map.emplace(m_key, std::move(m_held));
  }

  absent_factory(const absent_factory&) = delete;
  absent_factory& operator=(const absent_factory&) = delete;

  //! Whether there was anything to hide. A test asserting behaviour "without
  //! the factory" says nothing if the factory was never in this build -- the
  //! protocols are conditional, so this is worth checking rather than assuming.
  bool was_present() const noexcept { return bool(m_held); }

private:
  FactoryList& m_list;
  score::uuid_t m_key;
  std::unique_ptr<score::InterfaceBase> m_held;
};
}
