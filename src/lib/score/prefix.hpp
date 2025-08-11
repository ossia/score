#ifndef SCORE_PREFIX_HEADER
#define SCORE_PREFIX_HEADER

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(UNICODE)
#define UNICODE 1
#endif
#if !defined(_UNICODE)
#define _UNICODE 1
#endif
#include <windows.h>
#include <mmsystem.h>
#undef near
#undef far
#endif

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/AggregateCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreePath.hpp>
#include <score/model/tree/VariantBasedNode.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/IsTemplate.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/string_map.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value.hpp>

#include <QGraphicsItem>
#include <QWidget>

#include <fmt/format.h>

#include <cmath>
#include <wobjectimpl.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>
#include <verdigris>
#endif
