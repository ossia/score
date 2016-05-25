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
class LayerPresenter;
class ISCORE_LIB_PROCESS_EXPORT LayerContextMenu
{
    public:
        using ContextMenuFun = std::function<void(QMenu&, QPoint, QPointF, const Process::LayerPresenter&)>;
        LayerContextMenu(StringKey<LayerContextMenu> k):
            m_key{std::move(k)}
        {

        }

        StringKey<LayerContextMenu> key() const { return m_key; }

        std::vector<ContextMenuFun> functions;

        void build(QMenu& m, QPoint pt, QPointF ptf, const Process::LayerPresenter& proc) const
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

        auto& get()
        { return m_container; }
        auto& get() const
        { return m_container; }

    private:
        std::unordered_map<StringKey<LayerContextMenu>, LayerContextMenu> m_container;
};

template<typename T>
class MetaContextMenu;

}

#define ISCORE_PROCESS_DECLARE_CONTEXT_MENU(Type) \
namespace ContextMenus { class Type; } \
namespace Process { template<> \
class MetaContextMenu<ContextMenus::Type> final : public LayerContextMenu \
{ \
    public: \
        MetaContextMenu(): \
            Process::LayerContextMenu{static_key()} { } \
 \
        static StringKey<Process::LayerContextMenu> static_key() \
        { return StringKey<Process::LayerContextMenu>{ #Type }; } \
}; }
