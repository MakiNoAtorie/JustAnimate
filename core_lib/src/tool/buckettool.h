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

#ifndef BUCKETTOOL_H
#define BUCKETTOOL_H

#include "stroketool.h"

#include "bitmapimage.h"
#include "bitmapbucket.h"

class Layer;
class VectorImage;

class BucketTool : public BaseTool
{
    Q_OBJECT
public:
    explicit BucketTool(QObject* parent = nullptr);

    QCursor cursor() override;
    ToolType type() const override { return BUCKET; }

    ToolProperties& toolProperties() override { return mSettings.toolProperties(); }
    const BucketToolProperties& settings() const { return mSettings; }

    void loadSettings() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void paintBitmap();
    void paintVector(Layer* layer);

    void applyChanges();

    void setStrokeThickness(qreal width);
    void setColorTolerance(int tolerance);
    void setColorToleranceEnabled(bool enabled);
    void setFillExpand(int fillExpandValue);
    void setFillExpandEnabled(bool enabled);
    void setFillReferenceMode(int referenceMode);
    void setFillMode(int mode);
    void setFillCloseGaps(int pixels);

    QPointF getCurrentPoint() const;
    QPointF getCurrentPixel() const;

signals:
    void fillModeChanged(int mode);
    void fillCloseGapsChanged(int pixels);
    void fillReferenceModeChanged(int referenceMode);
    void fillExpandEnabledChanged(bool isON);
    void fillExpandChanged(int fillExpandValue);
    void toleranceEnabledChanged(bool isON);
    void toleranceChanged(int width);
    void strokeThicknessChanged(qreal width);

private:

    BitmapBucket mBitmapBucket;
    VectorImage* vectorImage = nullptr;

    bool mFilledOnMove = false;
    bool mDidFill = false;

    BucketToolProperties mSettings;
    StrokeInterpolator mInterpolator;
    std::optional<Edit> mFillEdit; // a drag can fill many times: one step from press to release
};

#endif // BUCKETTOOL_H
