#pragma once

#include <score/model/Identifier.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/document/Document.hpp>

#include <QActionGroup>
#include <QMainWindow>
#include <QPair>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>

#include <score_lib_base_export.h>

#include <vector>
#include <verdigris>

class QCloseEvent;
class QDockWidget;
class QEvent;
class QSplitter;
class QObject;
class QTabWidget;
class QLabel;
namespace score
{
struct PanelStatus;
class FixedTabWidget;
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
  void allPanelsAdded();

  void addTopToolbar(QToolBar* b);

public:
  void activeDocumentChanged(const Id<DocumentModel>& arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, activeDocumentChanged, arg_1)
  void closeRequested(const Id<DocumentModel>& arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, closeRequested, arg_1)

  void ready() E_SIGNAL(SCORE_LIB_BASE_EXPORT, ready)
  void sizeChanged(QSize arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sizeChanged, arg_1)

public:
  void on_fileNameChanged(DocumentView* d, const QString& newName);
  W_SLOT(on_fileNameChanged);

  QWidget* centralDocumentWidget{};
  QSplitter* rightSplitter{};
  QWidget* topleftToolbar{};
  FixedTabWidget* leftTabs{};
  // QTabWidget* rightTabs{};
  FixedTabWidget* bottomTabs{};
  QTabWidget* centralTabs{};
  QWidget* transportBar{};

private:
  bool event(QEvent* event) override;
  void changeEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;

  std::map<QWidget*, DocumentView*> m_documents;
  std::vector<QPair<PanelDelegate*, QWidget*>> m_leftPanels;
  std::vector<QPair<PanelDelegate*, QWidget*>> m_rightPanels;

  Presenter* m_presenter{};
  QLabel* m_status{};
};

}
