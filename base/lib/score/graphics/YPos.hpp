#pragma once
#include <score/tools/Todo.hpp>

#if defined(_WIN32)
#define SCORE_YPOS(normaly, windowsy) windowsy
#else
#define SCORE_YPOS(normaly, windowsy) normaly
#endif
