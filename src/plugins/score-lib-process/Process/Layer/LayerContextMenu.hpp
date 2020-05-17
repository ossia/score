#pragma once
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/std/HashMap.hpp>

#include <QPoint>
#include <QPointF>

#include <score_lib_process_export.h>

#include <functional>
class QMenu;

namespace Process
{
template <typename T>
class MetaContextMenu;
struct LayerContext;

using ContextMenuFun = std::function<void(QMenu&, QPoint, QPointF, const Process::LayerContext&)>;
class SCORE_LIB_PROCESS_EXPORT LayerContextMenu
{
public:
  LayerContextMenu(StringKey<LayerContextMenu> k);

  StringKey<LayerContextMenu> key() const { return m_key; }

  std::vector<ContextMenuFun> functions;

  void build(QMenu& m, QPoint pt, QPointF ptf, const Process::LayerContext& proc) const;

private:
  StringKey<LayerContextMenu> m_key;
};

class SCORE_LIB_PROCESS_EXPORT LayerContextMenuManager
{
public:
  void insert(LayerContextMenu val)
  {
    SCORE_ASSERT(m_container.find(val.key()) == m_container.end());
    m_container.insert(std::make_pair(val.key(), std::move(val)));
  }

  template <typename T>
  LayerContextMenu& menu()
  {
    using meta_t = MetaContextMenu<T>;
    SCORE_ASSERT(m_container.find(meta_t::static_key()) != m_container.end());
    return m_container.find(meta_t::static_key()).value();
  }

  template <typename T>
  const LayerContextMenu& menu() const
  {
    using meta_t = MetaContextMenu<T>;
    SCORE_ASSERT(m_container.find(meta_t::static_key()) != m_container.end());
    return m_container.find(meta_t::static_key()).value();
  }

  auto& get() { return m_container; }
  auto& get() const { return m_container; }

private:
  score::hash_map<StringKey<LayerContextMenu>, LayerContextMenu> m_container;
};
}

#define SCORE_PROCESS_DECLARE_CONTEXT_MENU(Export, Type)                      \
  namespace ContextMenus                                                      \
  {                                                                           \
  class Type;                                                                 \
  }                                                                           \
  namespace Process                                                           \
  {                                                                           \
  template <>                                                                 \
  class Export MetaContextMenu<ContextMenus::Type>                            \
  {                                                                           \
  public:                                                                     \
    static LayerContextMenu make() { return LayerContextMenu{static_key()}; } \
                                                                              \
    static StringKey<Process::LayerContextMenu> static_key()                  \
    {                                                                         \
      return StringKey<Process::LayerContextMenu>{#Type};                     \
    }                                                                         \
  };                                                                          \
  }
