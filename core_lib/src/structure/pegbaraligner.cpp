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
#include "pegbaraligner.h"

#include <QDebug>
#include <editor.h>
#include <pencilerror.h>

#include <bitmapimage.h>
#include <layerbitmap.h>
#include <layermanager.h>
#include <undoredomanager.h>

PegStatus::PegStatus(ErrorCode code, QPoint point)
    : Status(code), point(point)
{
}

PegBarAligner::PegBarAligner(Editor* editor, QRect searchRect) :
    mEditor(editor), mPegSearchRect(searchRect)
{
}


Status PegBarAligner::align(const QStringList& layers)
{
    // The reference is the keyframe shown at the current frame, which may be a held frame
    LayerBitmap* layerBitmap = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer());
    BitmapImage* img = layerBitmap->getLastBitmapImageAtFrame(mEditor->currentFrame());
    PegStatus result = img ? findPoint(*img) : PegStatus(Status::FAIL);

    if (!result.ok())
    {
        return Status(Status::FAIL, tr("Peg hole not found!\nCheck selection, and please try again.", "PegBar error message"));
    }

    const QPoint peg = result.point;

    // Find every offset first so a missing peg leaves the drawing untouched
    QList<BitmapOffset> offsets;
    for (int i = 0; i < layers.count(); i++)
    {
        layerBitmap = static_cast<LayerBitmap*>(mEditor->layers()->findLayerByName(layers.at(i)));
        if (mEditor->refusesEdit(layerBitmap)) { return Status::SAFE; }
        for (int k = layerBitmap->firstKeyFramePosition(); k <= layerBitmap->getMaxKeyFramePosition(); k++)
        {
            if (!layerBitmap->keyExists(k)) { continue; }

            img = layerBitmap->getBitmapImageAtFrame(k);
            result = findPoint(*img);
            if (!result.ok())
            {
                const QString errorDescription = tr("Peg bar not found at %2, %1").arg(k).arg(layerBitmap->name());
                return Status(result.code(), errorDescription);
            }
            if (result.point != peg)
            {
                offsets.append({ layerBitmap->id(), k, peg - result.point });
            }
        }
    }

    Edit edit = mEditor->edit(tr("Peg bar alignment"));
    for (const BitmapOffset& move : qAsConst(offsets))
    {
        layerBitmap = static_cast<LayerBitmap*>(mEditor->layers()->findLayerById(move.layerId));
        img = layerBitmap->getBitmapImageAtFrame(move.frame);
        img->enableAutoCrop(false);
        img->moveTopLeft(img->topLeft() + move.offset);
        emit mEditor->frameModified(move.frame);
    }
    mEditor->undoRedo()->recordBitmapMoves(offsets, tr("Peg bar alignment"));
    edit.commit();

    mEditor->deselectAll();

    return Status::OK;
}


PegStatus PegBarAligner::findPoint(const BitmapImage& image) const
{
    QPoint point;
    const int left = mPegSearchRect.left();
    const int right = mPegSearchRect.right();
    const int top = mPegSearchRect.top();
    const int bottom = mPegSearchRect.bottom();
    const int grayValue = mGrayThreshold;

    bool foundX = false;

    for (int x = left; x <= right && !foundX; x++)
    {
        for (int y = top; y <= bottom; y++)
        {
            const QRgb& scan = image.constScanLine(x,y);
            if (qAlpha(scan) == 255 && qGray(scan) < grayValue)
            {
                foundX = true;
                point.setX(x);

                break;
            }
        }
    }

    bool foundY = false;
    for (int y = top; y <= bottom && !foundY; y++)
    {
        for (int x = left; x <= right; x++)
        {
            const QRgb& scan = image.constScanLine(x,y);
            if (qAlpha(scan) == 255 && qGray(scan) < grayValue)
            {
                foundY = true;
                point.setY(y);

                break;
            }
        }
    }

    if (foundX && foundY) {
        return PegStatus(Status::OK, point);
    }
    return Status::FAIL;
}
