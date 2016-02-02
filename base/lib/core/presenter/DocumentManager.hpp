#pragma once
#include <core/document/DocumentBuilder.hpp>
#include <QObject>
#include <QString>
#include <algorithm>
#include <vector>
#include <iscore_lib_base_export.h>
class QRecentFilesMenu;
namespace iscore {
class Document;
struct ApplicationContext;
class View;
}  // namespace iscore

namespace iscore
{
class ISCORE_LIB_BASE_EXPORT DocumentManager : public QObject
{
        Q_OBJECT
    public:
        DocumentManager(
                iscore::View& view,
                QObject* parentPresenter);

        void init(const iscore::ApplicationContext& ctx);

        ~DocumentManager();

        auto recentFiles() const
        { return m_recentFiles; }

        // Document management
        Document* setupDocument(const iscore::ApplicationContext& ctx,
                                iscore::Document* doc);

        template<typename... Args>
        void newDocument(
                const iscore::ApplicationContext& ctx,
                Args&&... args)
        {
            prepareNewDocument(ctx);
            setupDocument(
                        ctx,
                        m_builder.newDocument(ctx, std::forward<Args>(args)...));
        }

        template<typename... Args>
        Document* loadDocument(
                const iscore::ApplicationContext& ctx,
                Args&&... args)
        {
            prepareNewDocument(ctx);
            return setupDocument(
                        ctx, m_builder.loadDocument(ctx, std::forward<Args>(args)...));
        }

        template<typename... Args>
        void restoreDocument(
                const iscore::ApplicationContext& ctx,
                Args&&... args)
        {
            prepareNewDocument(ctx);
            setupDocument(
                        ctx, m_builder.restoreDocument(ctx, std::forward<Args>(args)...));
        }

        // Restore documents after a crash
        void restoreDocuments(const iscore::ApplicationContext& ctx);

        const std::vector<Document*>& documents() const
        { return m_documents; }

        Document* currentDocument() const;
        void setCurrentDocument(
                const iscore::ApplicationContext& ctx,
                Document* doc);

        // Returns true if the document was closed.
        bool closeDocument(
                const iscore::ApplicationContext& ctx,
                Document&);
        void forceCloseDocument(
                const iscore::ApplicationContext& ctx,
                Document&);


        // Methods to save and load
        bool saveDocument(Document&);
        bool saveDocumentAs(Document&);

        bool saveStack();
        Document* loadStack(
                const iscore::ApplicationContext& ctx);
        Document* loadStack(
                const iscore::ApplicationContext& ctx,
                const QString&);

        Document* loadFile(
                const iscore::ApplicationContext& ctx);
        Document* loadFile(
                const iscore::ApplicationContext& ctx,
                const QString& filename);

        void prepareNewDocument(
                const iscore::ApplicationContext& ctx);

        bool closeAllDocuments(
                const iscore::ApplicationContext& ctx);

    private:
        /**
         * @brief checkAndUpdateJson
         * @return boolean indicating if the document is loadable
         */
        bool checkAndUpdateJson(
                QJsonDocument&,
                const iscore::ApplicationContext& ctx);

        iscore::View& m_view;

        DocumentBuilder m_builder;

        std::vector<Document*> m_documents;
        Document* m_currentDocument{};
        QPointer<QRecentFilesMenu> m_recentFiles{};


};
}

Id<iscore::DocumentModel> getStrongId(const std::vector<iscore::Document*>& v);
