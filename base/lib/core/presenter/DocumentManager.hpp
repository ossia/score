#pragma once
#include <core/document/DocumentBuilder.hpp>
#include <qobject.h>
#include <qstring.h>
#include <algorithm>
#include <vector>

class QRecentFilesMenu;
namespace iscore {
class Document;
}  // namespace iscore

namespace iscore
{
class Presenter;

class DocumentManager : public QObject
{
        Q_OBJECT
    public:
        DocumentManager(Presenter&);

        ~DocumentManager();

        auto recentFiles() const
        { return m_recentFiles; }

        // Document management
        Document* setupDocument(iscore::Document* doc);

        template<typename... Args>
        void newDocument(Args&&... args)
        {
            prepareNewDocument();
            setupDocument(m_builder.newDocument(std::forward<Args>(args)...));
        }

        template<typename... Args>
        Document* loadDocument(Args&&... args)
        {
            prepareNewDocument();
            return setupDocument(m_builder.loadDocument(std::forward<Args>(args)...));
        }

        template<typename... Args>
        void restoreDocument(Args&&... args)
        {
            prepareNewDocument();
            setupDocument(m_builder.restoreDocument(std::forward<Args>(args)...));
        }

        // Restore documents after a crash
        void restoreDocuments();

        const std::vector<Document*>& documents() const
        { return m_documents; }

        Document* currentDocument() const;
        void setCurrentDocument(Document* doc);

        // Returns true if the document was closed.
        bool closeDocument(Document&);


        // Methods to save and load
        bool saveDocument(Document&);
        bool saveDocumentAs(Document&);

        Document* loadFile();
        Document* loadFile(const QString& filename);

        void prepareNewDocument();

        bool closeAllDocuments();

    signals:
        void currentDocumentChanged(Document* newDoc);

    private:
        Presenter& m_presenter;

        DocumentBuilder m_builder;

        std::vector<Document*> m_documents;
        Document* m_currentDocument{};
        QRecentFilesMenu* m_recentFiles{};


};
}
