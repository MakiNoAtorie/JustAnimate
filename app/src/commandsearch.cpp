#include "commandsearch.h"

#include <QAction>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSet>
#include <QVBoxLayout>

#include "theme.h"

static QString plainText(const QAction* a)
{
    QString t = a->text();
    t.remove(QLatin1Char('&'));
    if (t.endsWith(QStringLiteral("...")))
    {
        t.chop(3);
    }
    return t.trimmed();
}

static void collect(const QList<QAction*>& actions, const QString& prefix, QSet<const QAction*>& seen, QList<CommandEntry>& out)
{
    for (QAction* a : actions)
    {
        if (a->isSeparator() || seen.contains(a)) { continue; }
        seen.insert(a);
        const QString name = plainText(a);
        if (name.isEmpty()) { continue; }
        const QString path = prefix.isEmpty() ? name : prefix + QStringLiteral(" > ") + name;
        if (a->menu())
        {
            collect(a->menu()->actions(), path, seen, out);
            continue;
        }
        out.append({ a, path, a->shortcut().toString(QKeySequence::NativeText), a->isEnabled() });
    }
}

QList<CommandEntry> collectCommands(const QMenuBar* menuBar)
{
    QList<CommandEntry> out;
    QSet<const QAction*> seen;
    collect(menuBar->actions(), QString(), seen, out);
    return out;
}

QList<CommandEntry> filterCommands(const QList<CommandEntry>& all, const QString& query)
{
    const QStringList words = query.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QList<CommandEntry> out;
    for (const CommandEntry& e : all)
    {
        bool match = true;
        for (const QString& w : words)
        {
            match = match && e.name.contains(w, Qt::CaseInsensitive);
        }
        if (match) { out.append(e); }
    }
    return out;
}

CommandSearchField::CommandSearchField(QWidget* parent) : QLineEdit(parent)
{
    setReadOnly(true);
    setPlaceholderText(tr("Search commands"));
    setMaximumWidth(180);
    addAction(Theme::icon(":/icons/tabler/search.svg"), QLineEdit::LeadingPosition);
}

void CommandSearchField::mousePressEvent(QMouseEvent*) { emit activated(); }

void CommandSearchField::focusInEvent(QFocusEvent* e)
{
    QLineEdit::focusInEvent(e);
    // A click opens through mousePressEvent; focus that returns when a window closes opens nothing.
    const Qt::FocusReason r = e->reason();
    if (r == Qt::TabFocusReason || r == Qt::BacktabFocusReason || r == Qt::ShortcutFocusReason) { emit activated(); }
}

CommandSearch::CommandSearch(const QList<CommandEntry>& all, QWidget* parent) : QDialog(parent), mAll(all)
{
    setWindowTitle(tr("Command search"));
    resize(520, 360);

    QVBoxLayout* layout = new QVBoxLayout(this);
    mEdit = new QLineEdit(this);
    mEdit->setPlaceholderText(tr("Type a command name"));
    mList = new QListWidget(this);
    mList->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(mEdit);
    layout->addWidget(mList);

    connect(mEdit, &QLineEdit::textChanged, this, &CommandSearch::refresh);
    connect(mEdit, &QLineEdit::returnPressed, this, [this] { runCurrent(); });
    connect(mList, &QListWidget::itemActivated, this, [this] { runCurrent(); });
    mEdit->installEventFilter(this);
    refresh();
}

bool CommandSearch::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == mEdit && e->type() == QEvent::KeyPress)
    {
        const int key = static_cast<QKeyEvent*>(e)->key();
        if ((key == Qt::Key_Down || key == Qt::Key_Up) && mList->count() > 0)
        {
            const int row = mList->currentRow() + (key == Qt::Key_Down ? 1 : -1);
            mList->setCurrentRow(qBound(0, row, mList->count() - 1));
            return true;
        }
    }
    return QDialog::eventFilter(obj, e);
}

void CommandSearch::refresh()
{
    mShown = filterCommands(mAll, mEdit->text());
    mList->clear();
    for (const CommandEntry& e : mShown)
    {
        QString text = e.name;
        if (!e.shortcut.isEmpty()) { text += QStringLiteral("    ") + e.shortcut; }
        QListWidgetItem* item = new QListWidgetItem(text, mList);
        if (!e.enabled) { item->setFlags(item->flags() & ~Qt::ItemIsEnabled); }
    }
    mList->setCurrentRow(0);
}

bool CommandSearch::runCurrent()
{
    const int row = mList->currentRow();
    if (row < 0 || row >= mShown.size() || !mShown[row].enabled) { return false; }
    QAction* action = mShown[row].action;
    accept();
    action->trigger();
    return true;
}
