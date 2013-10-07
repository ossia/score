#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "TTScoreAPI.h"
#include "TTModular.h"

class Engine
{
private:
  TTTimeProcessPtr    _mainScenario;                                 /// The top scenario

public:
  Engine();

private:
  void initModular();
  void initScore();
};

#endif // ENGINE_HPP
