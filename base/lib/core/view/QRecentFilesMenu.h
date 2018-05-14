#ifndef QRECENTFILESMENU_H
#define QRECENTFILESMENU_H
/*The MIT License (MIT)

Copyright (c) 2011 Morgan Leborgne

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <QMenu>
#include <QStringList>
#include <wobjectdefs.h>
class QRecentFilesMenu final : public QMenu
{
    W_OBJECT(QRecentFilesMenu)
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

    //!
    void addRecentFile(const QString &fileName);

    //! Removes all the menu's actions.
    void clearMenu();
    W_SLOT(clearMenu);

    //! Sets the maximum number of entries int he menu.
    void setMaxCount(int);
public:
    //! This signal is emitted when a recent file in this menu is triggered.
    void recentFileTriggered(const QString & filename)
    W_SIGNAL(recentFileTriggered, filename);

private:
    void menuTriggered(QAction*);
    W_SLOT(menuTriggered);
    void updateRecentFileActions();

    int m_maxCount{};
    QString m_format;
    QStringList m_files;
    W_PROPERTY(int, maxCount READ maxCount WRITE setMaxCount)
    W_PROPERTY(QString, format READ format WRITE setFormat)
};

#endif // QRECENTFILEMENU_H
