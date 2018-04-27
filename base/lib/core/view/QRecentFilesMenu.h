#ifndef QRECENTFILESMENU_H
#define QRECENTFILESMENU_H

#include <QMenu>
#include <QStringList>

class QRecentFilesMenu final : public QMenu
{
    Q_OBJECT
    Q_PROPERTY(int maxCount READ maxCount WRITE setMaxCount)
    Q_PROPERTY(QString format READ format WRITE setFormat)
public:
    //! Constructs a menu with parent parent.
    QRecentFilesMenu(QWidget * parent = 0 );

    //! Constructs a menu with a title and a parent.
    QRecentFilesMenu(const QString & title, QWidget * parent = 0 );

    //! Returns the maximum number of entries in the menu.
    int maxCount() const;

    /** This property holds the string used to generate the item text.
     * %d is replaced by the item number
     * %s is replaced by the item text
     * The default value is "%d %s".
     */
    void setFormat(const QString &format);

    //! Returns the current format. /sa setFormat
    const QString & format() const;

    /** Saves the state of the recent entries.
     * Typically this is used in conjunction with QSettings to remember entries
     * for a future session. A version number is stored as part of the data.
     * Here is an example:
     * QSettings settings;
     * settings.setValue("recentFiles", recentFilesMenu->saveState());
     */
    QByteArray saveState() const;

    /** Restores the recent entries to the state specified.
     * Typically this is used in conjunction with QSettings to restore entries from a past session.
     * Returns false if there are errors.
     * Here is an example:
     * QSettings settings;
     * recentFilesMenu->restoreState(settings.value("recentFiles").toByteArray());
     */
    bool restoreState(const QByteArray &state);

public Q_SLOTS:
    //!
    void addRecentFile(const QString &fileName);

    //! Removes all the menu's actions.
    void clearMenu();

    //! Sets the maximum number of entries int he menu.
    void setMaxCount(int);
Q_SIGNALS:
    //! This signal is emitted when a recent file in this menu is triggered.
    void recentFileTriggered(const QString & filename);

private Q_SLOTS:
    void menuTriggered(QAction*);
    void updateRecentFileActions();
private:
    int m_maxCount;
    QString m_format;
    QStringList m_files;
};

#endif // QRECENTFILEMENU_H
