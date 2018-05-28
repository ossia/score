#pragma once
#include <QColor>
#include <wobjectdefs.h>
#include <QLabel>
#include <QString>
#include <QWidget>
#include <list>
#include <memory>
#include <qnamespace.h>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score_lib_inspector_export.h>

class IdentifiedObjectAbstract;
class QVBoxLayout;

namespace score
{
struct DocumentContext;
class SelectionDispatcher;
}

namespace Inspector
{
/*!
 * \brief The InspectorWidgetBase class
 * Set the global structuration for an inspected element. Inherited by class
 * that implement specific type
 *
 * Manage sections added by user.
 */
class SCORE_LIB_INSPECTOR_EXPORT InspectorWidgetBase : public QWidget
{
  W_OBJECT(InspectorWidgetBase)
public:
  /*!
   * \brief InspectorWidgetBase Constructor
   * \param inspectedObj The selected object
   * \param parent The parent Widget
   */
  explicit InspectorWidgetBase(
      const IdentifiedObjectAbstract& inspectedObj,
      const score::DocumentContext& context,
      QWidget* parent,
      QString name);
  ~InspectorWidgetBase();

  const score::DocumentContext& context() const
  {
    return m_context;
  }

  void updateSectionsView(
      QVBoxLayout* layout, const std::vector<QWidget*>& contents);
  void updateAreaLayout(const std::vector<QWidget*>& contents);
  void updateAreaLayout(std::initializer_list<QWidget*> contents);

  void addHeader(QWidget* header);

  // Manage Values
  const IdentifiedObjectAbstract& inspectedObject() const;
  const IdentifiedObjectAbstract* inspectedObject_addr() const
  {
    return &inspectedObject();
  }

  // getters
  QVBoxLayout* areaLayout()
  {
    return m_scrollAreaLayout;
  }

  CommandDispatcher<>* commandDispatcher() const
  {
    return m_commandDispatcher;
  }

private:
  const IdentifiedObjectAbstract& m_inspectedObject;
  const score::DocumentContext& m_context;
  CommandDispatcher<>* m_commandDispatcher{};
  QVBoxLayout* m_scrollAreaLayout{};

  std::vector<QWidget*> m_sections;
  QColor _currentColor{Qt::gray};

  static const int m_colorIconSize{21};

  QVBoxLayout* m_layout{};
  QLabel* m_label{};
};
}
