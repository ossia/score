#pragma once
#include <functional>
#include <unordered_map>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_lib_process_export.h>

#include <QPoint>
#include <QPointF>
class QMenu;

namespace Process
{
template<typename T>
class MetaContextMenu;
class LayerContext;

using ContextMenuFun = std::function<void(QMenu&, QPoint, QPointF, const Process::LayerContext&)>;
class ISCORE_LIB_PROCESS_EXPORT LayerContextMenu
{
    public:
        LayerContextMenu(StringKey<LayerContextMenu> k):
            m_key{std::move(k)}
        {

        }

        StringKey<LayerContextMenu> key() const { return m_key; }

        std::vector<ContextMenuFun> functions;

        void build(QMenu& m, QPoint pt, QPointF ptf, const Process::LayerContext& proc) const
        {
            for(auto& fun : functions)
                fun(m, pt, ptf, proc);
        }

    private:
        StringKey<LayerContextMenu> m_key;
};

class ISCORE_LIB_PROCESS_EXPORT LayerContextMenuManager
{
    public:
        void insert(LayerContextMenu val)
        {
            ISCORE_ASSERT(m_container.find(val.key()) == m_container.end());
            m_container.insert(std::make_pair(val.key(), std::move(val)));
        }

        template<typename T>
        auto& menu()
        {
            using meta_t = MetaContextMenu<T>;
            ISCORE_ASSERT(m_container.find(meta_t::static_key()) != m_container.end());
            return m_container.find(meta_t::static_key())->second;
        }

        template<typename T>
        auto& menu() const
        {
            using meta_t = MetaContextMenu<T>;
            ISCORE_ASSERT(m_container.find(meta_t::static_key()) != m_container.end());
            return m_container.find(meta_t::static_key())->second;
        }

        auto& get()
        { return m_container; }
        auto& get() const
        { return m_container; }

    private:
        std::unordered_map<StringKey<LayerContextMenu>, LayerContextMenu> m_container;
};

}

#define ISCORE_PROCESS_DECLARE_CONTEXT_MENU(Type) \
namespace ContextMenus { class Type; } \
namespace Process { template<> \
class MetaContextMenu<ContextMenus::Type> \
{ \
    public: \
        static LayerContextMenu make() { return LayerContextMenu{ static_key() }; } \
 \
        static StringKey<Process::LayerContextMenu> static_key() \
        { return StringKey<Process::LayerContextMenu>{ #Type }; } \
}; }
