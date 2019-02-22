#pragma once
#include <score_player_export.h>

#include <atomic>
#include <string>
#include <thread>
namespace ossia
{
namespace net
{
class device_base;
}
}
namespace score
{
class PlayerImpl;
class SCORE_PLAYER_EXPORT Player
{
public:
  Player();
  Player(std::string plugin_path);
  ~Player();

  void setPort(int port);
  void load(std::string path);
  void play();
  void stop();
  void registerDevice(ossia::net::device_base&);

private:
  std::unique_ptr<PlayerImpl> m_player;
  std::thread m_thread;
  std::atomic_bool m_loaded{};
};
}
