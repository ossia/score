#include "player.hpp"

int main(int argc, char** argv)
{
  if(argc > 1)
  {
    iscore::Player p;
    p.load(argv[1]);
    p.play();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    p.stop();
  }
  return 0;
}
