#pragma once
#include <thread>
#include <atomic>
#include <string>

#include <iscore_player_export.h>
namespace ossia { namespace net { class device_base; } }
namespace iscore
{
class PlayerImpl;
class ISCORE_PLAYER_EXPORT Player
{
public:
  Player();
  ~Player();

  void load(std::string path);
  void play();
  void stop();
  void registerDevice(ossia::net::device_base&);

private:
  std::unique_ptr<PlayerImpl> m_player;
  std::atomic_bool m_loaded{};
  std::thread m_thread;
};

}
