#pragma once
#include <QObject>
#include <QShortcut>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore_lib_base_export.h>

#include <boost/optional.hpp>
class Selection;
namespace iscore
{
struct ApplicationContext;
struct DocumentContext;
class PanelModel;
class PanelView;

struct PanelStatus
{
        PanelStatus(
                bool isShown,
                Qt::DockWidgetArea d,
                int prio,
                QString name,
                const QKeySequence& sc):
            shown{isShown},
            dock{d},
            priority{prio},
            prettyName{std::move(name)},
            shortcut(sc)
        {}

        const bool shown; // Controls if it is shown by default.
        const Qt::DockWidgetArea dock; // Which dock.
        const int priority;  // Higher priority will come up first.
        const QString prettyName; // Used in the headeer.
        const QKeySequence shortcut; // Keyboard shortcut to show or hide the panel.
};

class ISCORE_LIB_BASE_EXPORT PanelDelegate
{
    public:
        PanelDelegate(
                const iscore::ApplicationContext& ctx):
            m_context{ctx}
        {

        }

        void setModel(const iscore::DocumentContext& model)
        {
            auto old = m_model;
            m_model = model;
            on_modelChanged(old, m_model);
        }

        void setModel(decltype(iscore::none) n)
        {
            auto old = m_model;
            m_model = boost::none;
            on_modelChanged(old, m_model);
        }

        auto document() const
        { return m_model; }


        virtual ~PanelDelegate();
        virtual QWidget* widget() = 0;
        virtual const PanelStatus& defaultPanelStatus() const = 0;

        virtual void setNewSelection(
                const Selection& s)
        {

        }

    protected:
        virtual void on_modelChanged(
                MaybeDocument oldm,
                MaybeDocument newm)
        {

        }

    private:
        const iscore::ApplicationContext& m_context;
        MaybeDocument m_model;
};
}
