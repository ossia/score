#pragma once
#include <score/model/Component.hpp>
#include <score/model/EntityMap.hpp>

#if !defined(SCORE_ALL_UNITY)
extern template class SCORE_LIB_BASE_EXPORT score::EntityMap<score::Component>;
#endif

#include <score/model/Entity.hpp>
