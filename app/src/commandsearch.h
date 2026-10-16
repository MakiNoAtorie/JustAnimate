#ifndef COMMANDSEARCH_H
#define COMMANDSEARCH_H

#include <QDialog>
#include <QLineEdit>
#include <QList>
#include <QString>

class QAction;
class QListWidget;
class QMenuBar;

struct CommandEntry
{
    QAction* action = nullptr;
    QString name;      // menu path, for example "Edit > Copy"
    QString shortcut;  // native text, empty when unbound
    bool enabled = true;
};

// Every command in the menus, submenus included, once each.
QList<CommandEntry> collectCommands(const QMenuBar* menuBar);

// Entries whose name contains every word of the query, case-insensitive.
QList<CommandEntry> filterCommands(const QList<CommandEntry>& all, const QString& query);

// Search field for the Command bar: a click or focus opens Command search.
class CommandSearchField : public QLineEdit
{
    Q_OBJECT
public:
    explicit CommandSearchField(QWidget* parent = nullptr);

signals:
    void activated();

protected:
    void mousePressEvent(QMouseEvent*) override;
    void focusInEvent(QFocusEvent*) override;
};

class CommandSearch : public QDialog
{
    Q_OBJECT
public:
    CommandSearch(const QList<CommandEntry>& all, QWidget* parent);

    // Runs the selected command if it is enabled; returns whether it ran.
    bool runCurrent();

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
    void refresh();

    QList<CommandEntry> mAll;
    QList<CommandEntry> mShown;
    QLineEdit* mEdit = nullptr;
    QListWidget* mList = nullptr;
};

#endif // COMMANDSEARCH_H
