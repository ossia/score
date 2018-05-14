#pragma once

#include <QMainWindow>
#include <core/document/Document.hpp>
#include <wobjectdefs.h>
#include <QPair>
#include <QString>
#include <vector>

class QCloseEvent;
class QDockWidget;
class QEvent;
class QObject;
class QTabWidget;

#include <score/model/Identifier.hpp>
#include <score_lib_base_export.h>

namespace score
{
class DocumentModel;
class DocumentView;
class PanelView;
class PanelDelegate;
class Presenter;
struct ApplicationContext;
/**
 * @brief The main display of the application.
 */
class SCORE_LIB_BASE_EXPORT View final : public QMainWindow
{
  W_OBJECT(View)
public:
  explicit View(QObject* parent);
  ~View() override;

  void setPresenter(Presenter*);

  void addDocumentView(score::DocumentView*);
  void setupPanel(PanelDelegate* v);

  void closeDocument(score::DocumentView* doc);
  void restoreLayout();
  void closeEvent(QCloseEvent*) override;

public:
  void activeDocumentChanged(const Id<DocumentModel>& arg_1) W_SIGNAL(activeDocumentChanged, arg_1);
  void closeRequested(const Id<DocumentModel>& arg_1) W_SIGNAL(closeRequested, arg_1);

  void ready() W_SIGNAL(ready);
  void sizeChanged(QSize arg_1) W_SIGNAL(sizeChanged, arg_1);

public:
  void on_fileNameChanged(DocumentView* d, const QString& newName); W_SLOT(on_fileNameChanged);

private:
  void changeEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;

  std::map<QWidget*, DocumentView*> m_documents;
  std::vector<QPair<PanelDelegate*, QDockWidget*>> m_leftPanels;
  std::vector<QPair<PanelDelegate*, QDockWidget*>> m_rightPanels;

  Presenter* m_presenter{};
  QTabWidget* m_tabWidget{};
};
}
