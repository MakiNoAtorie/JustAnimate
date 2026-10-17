/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "cameracontextmenu.h"

#include <functional>

#include "cameraeasingtype.h"
#include "layercamera.h"
#include "camera.h"
#include "editor.h"
#include "undoredomanager.h"

CameraContextMenu::CameraContextMenu(Editor* editor, int frameNumber, const LayerCamera* layer) :
    mFrameNumber(frameNumber), mCurrentLayer(layer)

{
    // Each menu edit is one undo step.
    auto undoable = [editor, layer](const QString& description, std::function<void()> change) {
        return [=] {
            if (editor->refusesEdit(layer)) { return; }
            const SAVESTATE_ID state = editor->undoRedo()->createState(UndoRedoRecordType::CAMERA_MODIFY, layer);
            change();
            editor->undoRedo()->record(state, description);
        };
    };
    const QString easingText = tr("Camera Easing");
    const QString transformText = tr("Camera Transform");
    auto ease = [=](CameraEasingType type) {
        return undoable(easingText, [=] { layer->setCameraEasingAtFrame(type, frameNumber); });
    };
    auto reset = [=](CameraFieldOption option) {
        return undoable(transformText, [=] { layer->resetCameraAtFrame(option, frameNumber); });
    };

    int nextFrame = layer->getNextKeyFramePosition(frameNumber);

    QMenu* cameraInterpolationMenu = addMenu(tr("Easing: frame %1 to %2").arg(frameNumber).arg(nextFrame));

    cameraInterpolationMenu->setEnabled(layer->getMaxKeyFramePosition() != frameNumber);

    Camera* selectedKey = layer->getCameraAtFrame(frameNumber);
    if (selectedKey != nullptr) {
        QAction* selectedAction = cameraInterpolationMenu->addAction(tr("Selected: ") + getInterpolationText(selectedKey->getEasingType()));
        selectedAction->setDisabled(true);
    }

    cameraInterpolationMenu->addAction(tr("Linear"), ease(CameraEasingType::LINEAR));
    cameraInterpolationMenu->addSeparator();
    QMenu* inMenu = cameraInterpolationMenu->addMenu(tr("In"));
    QMenu* outMenu = cameraInterpolationMenu->addMenu(tr("Out"));
    QMenu* inOutMenu = cameraInterpolationMenu->addMenu(tr("In-Out"));
    QMenu* outInMenu = cameraInterpolationMenu->addMenu(tr("Out-In"));

    QString slow = tr("Slow");
    QString moderate = tr("Moderate");
    QString quick = tr("Quick");
    QString fast = tr("Fast");
    QString faster = tr("Faster");
    QString fastest = tr("Fastest");
    QString circleBased = tr("Circle-based");
    QString overshoot = tr("Overshoot");
    QString elastic = tr("Elastic");
    QString bounce = tr("Bounce");

    inMenu->addAction(slow, ease(CameraEasingType::INSINE));
    outMenu->addAction(slow, ease(CameraEasingType::OUTSINE));
    inOutMenu->addAction(slow, ease(CameraEasingType::INOUTSINE));
    outInMenu->addAction(slow, ease(CameraEasingType::OUTINSINE));
    inMenu->addAction(moderate, ease(CameraEasingType::INQUAD));
    outMenu->addAction(moderate, ease(CameraEasingType::OUTQUAD));
    inOutMenu->addAction(moderate, ease(CameraEasingType::INOUTQUAD));
    outInMenu->addAction(moderate, ease(CameraEasingType::OUTINQUAD));
    inMenu->addAction(quick, ease(CameraEasingType::INCUBIC));
    outMenu->addAction(quick, ease(CameraEasingType::OUTCUBIC));
    inOutMenu->addAction(quick, ease(CameraEasingType::INOUTCUBIC));
    outInMenu->addAction(quick, ease(CameraEasingType::OUTINCUBIC));
    inMenu->addAction(fast, ease(CameraEasingType::INQUART));
    outMenu->addAction(fast, ease(CameraEasingType::OUTQUART));
    inOutMenu->addAction(fast, ease(CameraEasingType::INOUTQUART));
    outInMenu->addAction(fast, ease(CameraEasingType::OUTINQUART));
    inMenu->addAction(faster, ease(CameraEasingType::INQUINT));
    outMenu->addAction(faster, ease(CameraEasingType::OUTQUINT));
    inOutMenu->addAction(faster, ease(CameraEasingType::INOUTQUINT));
    outInMenu->addAction(faster, ease(CameraEasingType::OUTINQUINT));
    inMenu->addAction(fastest, ease(CameraEasingType::INEXPO));
    outMenu->addAction(fastest, ease(CameraEasingType::OUTEXPO));
    inOutMenu->addAction(fastest, ease(CameraEasingType::INOUTEXPO));
    outInMenu->addAction(fastest, ease(CameraEasingType::OUTINEXPO));
    inMenu->addAction(circleBased, ease(CameraEasingType::INCIRC));
    outMenu->addAction(circleBased, ease(CameraEasingType::OUTCIRC));
    inOutMenu->addAction(circleBased, ease(CameraEasingType::INOUTCIRC));
    outInMenu->addAction(circleBased, ease(CameraEasingType::OUTINCIRC));
    inMenu->addAction(overshoot, ease(CameraEasingType::INBACK));
    outMenu->addAction(overshoot, ease(CameraEasingType::OUTBACK));
    inOutMenu->addAction(overshoot, ease(CameraEasingType::INOUTBACK));
    outInMenu->addAction(overshoot, ease(CameraEasingType::OUTINBACK));
    inMenu->addAction(elastic, ease(CameraEasingType::INELASTIC));
    outMenu->addAction(elastic, ease(CameraEasingType::OUTELASTIC));
    inOutMenu->addAction(elastic, ease(CameraEasingType::INOUTELASTIC));
    outInMenu->addAction(elastic, ease(CameraEasingType::OUTINELASTIC));
    inMenu->addAction(bounce, ease(CameraEasingType::INBOUNCE));
    outMenu->addAction(bounce, ease(CameraEasingType::OUTBOUNCE));
    inOutMenu->addAction(bounce, ease(CameraEasingType::INOUTBOUNCE));
    outInMenu->addAction(bounce, ease(CameraEasingType::OUTINBOUNCE));

    QMenu* cameraFieldMenu = addMenu(tr("Transform"));
    cameraFieldMenu->addAction(tr("Reset all"), reset(CameraFieldOption::RESET_FIELD));
    cameraFieldMenu->addSeparator();
    cameraFieldMenu->addAction(tr("Reset position"), reset(CameraFieldOption::RESET_TRANSLATION));
    cameraFieldMenu->addAction(tr("Reset scale"), reset(CameraFieldOption::RESET_SCALING));
    cameraFieldMenu->addAction(tr("Reset rotation"), reset(CameraFieldOption::RESET_ROTATION));
    cameraFieldMenu->addSeparator();
    QAction* alignHAction = cameraFieldMenu->addAction(tr("Align horizontally to frame %1").arg(nextFrame), reset(CameraFieldOption::ALIGN_HORIZONTAL));
    QAction* alignVAction = cameraFieldMenu->addAction(tr("Align vertically to frame %1").arg(nextFrame), reset(CameraFieldOption::ALIGN_VERTICAL));
    cameraFieldMenu->addSeparator();
    QAction* holdAction = cameraFieldMenu->addAction(tr("Hold to frame %1").arg(nextFrame), reset(CameraFieldOption::HOLD_FRAME));
    if (frameNumber == layer->getMaxKeyFramePosition()) {
        holdAction->setDisabled(true);
        alignHAction->setDisabled(true);
        alignVAction->setDisabled(true);
    }
}
