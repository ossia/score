#include "player.hpp"

int main(int argc, char** argv)
{
  iscore::player x{argc, argv};
  x.load("/home/jcelerier/i-score/Tests/testdata/execution.scorejson");
  x.play();
  std::this_thread::sleep_for(std::chrono::seconds(20));
  return 0;
}
