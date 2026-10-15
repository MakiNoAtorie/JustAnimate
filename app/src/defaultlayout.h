#ifndef DEFAULTLAYOUT_H
#define DEFAULTLAYOUT_H

#include <QList>
#include <QSize>

class QDockWidget;
class QMainWindow;

struct DefaultDockLayout
{
    QList<QDockWidget*> left;
    QList<QDockWidget*> right;
    QDockWidget* bottom = nullptr;
};

// Docks every widget in its default area and sizes the areas for a screen of the given size.
void applyDefaultDockLayout(QMainWindow* window, const DefaultDockLayout& docks, QSize screen);

#endif // DEFAULTLAYOUT_H
