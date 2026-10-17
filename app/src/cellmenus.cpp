/*

JustAnimate - Traditional Animation Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "cellmenus.h"

#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>

#include "attributes.h"
#include "cameracontextmenu.h"
#include "editor.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "object.h"
#include "preferencemanager.h"
#include "objectdata.h"
#include "pencildef.h"

namespace CellMenus
{

void showCellMenu(Editor* editor, Layer* layer, int frame, const QPoint& globalPos)
{
    LayerManager* layers = editor->layers();
    if (layers->currentLayer() != layer) { layers->setCurrentLayer(layer); }

    if (layer->type() == Layer::SOUND)
    {
        showMarkerMenu(editor, layer->id(), frame, globalPos);
        return;
    }
    if (!layer->keyExists(frame)) { return; }
    if (layer->type() == Layer::CAMERA)
    {
        CameraContextMenu menu(editor, frame, static_cast<const LayerCamera*>(layer));
        menu.exec(globalPos);
        if (layer->getKeyFrameAt(frame)->isModified()) { emit editor->frameModified(frame); }
        return;
    }
    if (!layer->isPaintable()) { return; }

    const KeyFrame* key = layer->getKeyFrameAt(frame);
    const TweenState state = key->tweenState();
    const CameraEasingType easing = key->tweenEasing();
    QMenu menu;
    QAction* toggle = menu.addAction(QCoreApplication::translate("CellMenus", "Tween to Next Key"));
    toggle->setCheckable(true);
    toggle->setChecked(key->tween());
    QObject::connect(toggle, &QAction::triggered, [=](bool on) { layers->setKeyTween(frame, on, easing, state); });
    addEasingMenu(&menu, QCoreApplication::translate("CellMenus", "Tween Easing"), easing,
                  [=](CameraEasingType type) { layers->setKeyTween(frame, true, type, state); });
    menu.addSeparator();

    // The Mark actions' labels; these act on the clicked Drawing, not the playhead.
    const std::pair<Mark, QString> marks[] = {
        { Mark::Key, QCoreApplication::translate("MainWindow2", "Key") },
        { Mark::Breakdown, QCoreApplication::translate("MainWindow2", "Breakdown") },
        { Mark::Inbetween, QCoreApplication::translate("MainWindow2", "Inbetween") },
        { Mark::None, QCoreApplication::translate("MainWindow2", "Clear Mark") },
    };
    QActionGroup* markGroup = new QActionGroup(&menu);
    for (const auto& [mark, text] : marks)
    {
        QAction* action = markGroup->addAction(menu.addAction(text));
        action->setCheckable(mark != Mark::None);
        action->setChecked(mark != Mark::None && key->mark() == mark);
        QObject::connect(action, &QAction::triggered, [=, mark = mark] { layers->setKeyMark(frame, mark); });
    }
    if (layer->isLinked(key))
    {
        menu.addSeparator();
        QObject::connect(menu.addAction(QCoreApplication::translate("CellMenus", "Unlink Drawing")), &QAction::triggered,
                         [=] { layers->unlinkDrawing(frame); });
    }
    menu.exec(globalPos);
}

void showLayerMenu(Editor* editor, Layer* layer, const QPoint& globalPos)
{
    LayerManager* layers = editor->layers();
    Object* object = editor->object();
    QMenu menu;
    if (layer->type() == Layer::BITMAP)
    {
        auto* bitmap = static_cast<LayerBitmap*>(layer);
        QAction* lockAlpha = menu.addAction(QCoreApplication::translate("CellMenus", "Lock Transparency"));
        lockAlpha->setCheckable(true);
        lockAlpha->setChecked(bitmap->lockAlpha());
        QObject::connect(lockAlpha, &QAction::triggered, [=](bool on) {
            layers->setAttribute(bitmap, AttributeId::LockAlpha, on);
        });
    }
    if (layer->isPaintable())
    {
        QAction* rough = menu.addAction(QCoreApplication::translate("CellMenus", "Rough Layer"));
        rough->setCheckable(true);
        rough->setChecked(layer->isRough());
        QObject::connect(rough, &QAction::triggered, [=](bool on) { layers->setRough(layer, on); });
        QAction* storyboard = menu.addAction(QCoreApplication::translate("CellMenus", "Storyboard Layer"));
        storyboard->setCheckable(true);
        storyboard->setChecked(layer->isStoryboard());
        QObject::connect(storyboard, &QAction::triggered, [=](bool on) { layers->setStoryboard(layer, on); });
        QMenu* blend = menu.addMenu(QCoreApplication::translate("CellMenus", "Blend Mode"));
        QActionGroup* group = new QActionGroup(blend);
        for (int mode = 0; mode < kBlendModeCount; ++mode)
        {
            QAction* action = group->addAction(blend->addAction(QCoreApplication::translate("BlendMode", kBlendModeNames[mode])));
            action->setCheckable(true);
            action->setChecked(static_cast<int>(layer->blendMode()) == mode);
            QObject::connect(action, &QAction::triggered, [=] { layers->setBlendMode(layer, static_cast<BlendMode>(mode)); });
        }
    }
    if (layer->type() != Layer::SOUND)
    {
        QMenu* timesheet = menu.addMenu(QCoreApplication::translate("CellMenus", "Timesheet Column"));
        QActionGroup* group = new QActionGroup(timesheet);
        for (int c = 0; c <= static_cast<int>(TimesheetColumn::Camera); ++c)
        {
            const TimesheetColumn column = static_cast<TimesheetColumn>(c);
            if (column == TimesheetColumn::Dialogue) { continue; } // sound layers only
            QAction* action = group->addAction(timesheet->addAction(Layer::timesheetColumnName(column)));
            action->setCheckable(true);
            action->setChecked(layer->timesheetColumn() == column);
            QObject::connect(action, &QAction::triggered, [=] { layers->setTimesheetColumn(layer, column); });
        }
    }

    menu.addSeparator();
    const LayerGroup* current = object->findLayerGroup(layer->groupId());
    if (current == nullptr)
    {
        QObject::connect(menu.addAction(QCoreApplication::translate("CellMenus", "New Group")), &QAction::triggered,
                         [=] { layers->createLayerGroup(layer); });
    }
    QMenu* addTo = menu.addMenu(QCoreApplication::translate("CellMenus", "Add to Group"));
    for (const LayerGroup& g : object->layerGroups())
    {
        if (g.id == layer->groupId() || object->layerGroupRange(g.id).first < 0) { continue; }
        const int id = g.id;
        QObject::connect(addTo->addAction(g.name), &QAction::triggered, [=] { layers->addLayerToGroup(layer, id); });
    }
    addTo->setEnabled(!addTo->isEmpty());
    if (current)
    {
        const int id = current->id;
        const bool collapsed = current->collapsed;
        const qreal opacity = current->opacity;
        QObject::connect(menu.addAction(QCoreApplication::translate("CellMenus", "Remove from Group")), &QAction::triggered,
                         [=] { layers->removeLayerFromGroup(layer); });
        QObject::connect(menu.addAction(collapsed ? QCoreApplication::translate("CellMenus", "Expand Group")
                                                  : QCoreApplication::translate("CellMenus", "Collapse Group")),
                         &QAction::triggered, [=] { layers->setLayerGroupCollapsed(id, !collapsed); });
        QAction* visible = menu.addAction(QCoreApplication::translate("CellMenus", "Show Group"));
        visible->setCheckable(true);
        visible->setChecked(current->visible);
        QObject::connect(visible, &QAction::triggered, [=](bool on) { layers->setLayerGroupVisible(id, on); });
        QObject::connect(menu.addAction(QCoreApplication::translate("CellMenus", "Group Opacity...")), &QAction::triggered, [=] {
            bool ok = false;
            const int percent = QInputDialog::getInt(QApplication::activeWindow(), QCoreApplication::translate("CellMenus", "Group Opacity"),
                                                     QCoreApplication::translate("CellMenus", "Opacity (%):"),
                                                     qRound(opacity * 100), 0, 100, 1, &ok);
            if (ok) { layers->setLayerGroupOpacity(id, percent / 100.0); }
        });
    }
    menu.exec(globalPos);
}

void showMarkerMenu(Editor* editor, int layerId, int frame, const QPoint& globalPos)
{
    ObjectData* data = editor->object()->data();
    LayerManager* layers = editor->layers();
    const bool has = data->markers(layerId).contains(frame);
    FrameMarker m = data->markers(layerId).value(frame, FrameMarker{ QString(), QColor(255, 140, 0) }); // palette-exempt: saved Marker colour
    QWidget* parent = QApplication::activeWindow();

    QMenu menu;
    QObject::connect(menu.addAction(has ? QCoreApplication::translate("CellMenus", "Rename Marker...")
                                        : QCoreApplication::translate("CellMenus", "Add Marker...")),
                     &QAction::triggered, [&] {
        bool ok = false;
        const QString name = QInputDialog::getText(parent, QCoreApplication::translate("CellMenus", "Marker"),
                                                   QCoreApplication::translate("CellMenus", "Name:"), QLineEdit::Normal, m.name, &ok);
        if (!ok) { return; }
        m.name = name;
        layers->setMarker(layerId, frame, m);
    });
    if (has)
    {
        QObject::connect(menu.addAction(QCoreApplication::translate("CellMenus", "Change Marker Color...")), &QAction::triggered, [&] {
            const QColor c = QColorDialog::getColor(m.color, parent);
            if (!c.isValid()) { return; }
            m.color = c;
            layers->setMarker(layerId, frame, m);
        });
        QObject::connect(menu.addAction(QCoreApplication::translate("CellMenus", "Move Marker...")), &QAction::triggered, [&] {
            bool ok = false;
            const int to = QInputDialog::getInt(parent, QCoreApplication::translate("CellMenus", "Move Marker"),
                                                QCoreApplication::translate("CellMenus", "To frame:"), frame, 1, MaxFramesBound, 1, &ok);
            if (ok) { layers->moveMarker(layerId, frame, to); }
        });
        QObject::connect(menu.addAction(QCoreApplication::translate("CellMenus", "Remove Marker")), &QAction::triggered,
                         [&] { layers->removeMarker(layerId, frame); });
    }
    menu.exec(globalPos);
}

void fillEasingMenu(QMenu* menu, CameraEasingType checked, const std::function<void(CameraEasingType)>& pick)
{
    QActionGroup* group = new QActionGroup(menu);
    for (int i = 0; i <= static_cast<int>(CameraEasingType::OUTINBOUNCE); ++i)
    {
        const CameraEasingType type = static_cast<CameraEasingType>(i);
        QAction* action = group->addAction(menu->addAction(QIcon(easingPreview(type)), getInterpolationText(type)));
        action->setCheckable(true);
        action->setChecked(type == checked);
        QObject::connect(action, &QAction::triggered, [=] { pick(type); });
    }
}

QMenu* addEasingMenu(QMenu* menu, const QString& title, CameraEasingType checked, const std::function<void(CameraEasingType)>& pick)
{
    QMenu* easing = menu->addMenu(title);
    fillEasingMenu(easing, checked, pick);
    return easing;
}

void fillInbetweenEasingMenu(QMenu* menu, Editor* editor, const std::function<void(CameraEasingType)>& pick)
{
    PreferenceManager* prefs = editor->preference();
    const int last = qBound(0, prefs->getInt(SETTING::INBETWEEN_EASING), static_cast<int>(CameraEasingType::OUTINBOUNCE));
    fillEasingMenu(menu, static_cast<CameraEasingType>(last), [=](CameraEasingType type) {
        prefs->set(SETTING::INBETWEEN_EASING, static_cast<int>(type));
        pick(type);
    });
    // Enter accepts the preselected easing.
    for (QAction* action : menu->actions()) { if (action->isChecked()) { menu->setActiveAction(action); } }
}

bool chooseInbetweenEasing(Editor* editor, const QPoint& globalPos, CameraEasingType& easing)
{
    QMenu menu;
    bool picked = false;
    fillInbetweenEasingMenu(&menu, editor, [&](CameraEasingType type) { easing = type; picked = true; });
    menu.exec(globalPos);
    return picked;
}
QPixmap easingPreview(CameraEasingType easing)
{
    const int size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QApplication::palette().color(QPalette::WindowText), 1.2));
    // Progress runs bottom to top; elastic and back curves may leave the box and clip.
    QPolygonF curve;
    for (int i = 0; i <= size; ++i)
    {
        const qreal t = qreal(i) / size;
        curve << QPointF(1 + t * (size - 2), size - 1 - easedProgress(easing, t) * (size - 2));
    }
    painter.drawPolyline(curve);
    return pixmap;
}

}
