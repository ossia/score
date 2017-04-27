#pragma once
#include <thread>
#include <atomic>
#include <string>

namespace iscore
{
class PlayerImpl;
class player
{
public:
  player(int& argc, char**& argv);
  ~player();

  void load(std::string path);
  void play();
  void stop();

private:
  std::unique_ptr<PlayerImpl> m_player;
  std::atomic_bool m_loaded{};
  std::thread m_thread;
};

}
