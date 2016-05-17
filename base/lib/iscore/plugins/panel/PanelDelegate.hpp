#pragma once
#include <QObject>
#include <QShortcut>
#include <iscore/document/DocumentContext.hpp>
#include <boost/optional.hpp>
#include <iscore_lib_base_export.h>

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
                const QString& name,
                const QKeySequence& sc):
            shown{isShown},
            dock{d},
            priority{prio},
            prettyName{name},
            shortcut(sc)
        {}

        const bool shown; // Controls if it is shown by default.
        const Qt::DockWidgetArea dock; // Which dock.
        const int priority;  // Higher priority will come up first.
        const QString prettyName; // Used in the headeer.
        const QKeySequence shortcut; // Keyboard shortcut to show or hide the panel.
};

class ISCORE_LIB_BASE_EXPORT PanelDelegate :
        public QObject
{
        Q_OBJECT
    public:
        using maybe_document_t = boost::optional<const iscore::DocumentContext&>;
        PanelDelegate(
                const iscore::ApplicationContext& ctx,
                QObject* parent):
            QObject{parent},
            m_context{ctx}
        {

        }

        virtual ~PanelDelegate();

        virtual QWidget* widget() = 0;
        virtual const PanelStatus& defaultPanelStatus() const = 0;

        void setModel(maybe_document_t model)
        {
            auto old = m_model;
            m_model = model;
            on_modelChanged(old, model);
        }

        auto document() const
        { return m_model; }

    protected:
        virtual void on_modelChanged(
                maybe_document_t oldm,
                maybe_document_t newm) = 0;

    private:
        const iscore::ApplicationContext& m_context;
        maybe_document_t m_model;
};
}

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
// MOVEME

namespace iscore
{
// MOVEME
class ISCORE_LIB_BASE_EXPORT PanelDelegateFactory :
        public iscore::AbstractFactory<PanelDelegateFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                PanelDelegateFactory,
                "8d6211f7-5244-44f9-94dd-f3e32255c43e")
        public:
            virtual ~PanelDelegateFactory();

        virtual PanelDelegate* make(
                const iscore::ApplicationContext& ctx,
                QObject* parent) = 0;
};

class ISCORE_LIB_BASE_EXPORT PanelDelegateFactoryList final :
        public ConcreteFactoryList<iscore::PanelDelegateFactory>
{
    public:
        using object_type = PanelDelegate;
};
}
