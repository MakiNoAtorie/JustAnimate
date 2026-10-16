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
#ifndef BRUSHPRESETEDITOR_H
#define BRUSHPRESETEDITOR_H

#include <memory>

#include <QDialog>
#include <QImage>

#include "brushpresets.h"

class BrushTool;
class Editor;
class LayerBitmap;
class QCheckBox;
class QLabel;
class QLineEdit;
class PressureCurveWidget;
class ScribbleArea;

/// A hidden editor whose Brush paints sample strokes, so samples use the real dab engine.
struct PresetSampler
{
    explicit PresetSampler(const QColor& ink);
    ~PresetSampler();

    /// Paints an S stroke with the brush's current settings, scaled to fit `size`.
    QImage paint(QSize size);
    /// Applies the preset to the brush, then paints.
    QImage paint(const BrushPresets& presets, const QString& name, QSize size);

    std::unique_ptr<ScribbleArea> area;
    std::unique_ptr<Editor> editor; // destroyed before the area
    LayerBitmap* layer = nullptr;
    BrushTool* brush = nullptr;
};

/// Edits every setting of a brush preset, its tip, pressure curve and description,
/// with a live sample stroke.
class BrushPresetEditor : public QDialog
{
    Q_OBJECT

public:
    /// Loads the settings of `source` (empty = tool defaults) and proposes `name`.
    /// `original` is the preset being edited, which save() may overwrite or rename;
    /// empty for a new or duplicated preset. A new preset is for `tool` (a `BrushPresets::toolKey`,
    /// empty = brush); otherwise the preset keeps the tool of `source`.
    BrushPresetEditor(const BrushPresets& presets, const QString& source, const QString& name,
                      const QString& original = QString(), const QString& tool = QString(), QWidget* parent = nullptr);
    ~BrushPresetEditor() override;

    /// Writes the preset. Fails, with a message in the dialog, when the name is
    /// invalid or names another existing preset.
    bool save();
    void accept() override;

    QImage preview() const { return mPreview; }
    QString name() const;

    static QSize previewSize() { return QSize(320, 80); }

private:
    /// The row label of a tool setting; the identifier for a setting without one.
    static QString settingLabel(int key, const QString& identifier);
    void updatePreview();
    void applyCurve();

    BrushPresets mPresets;
    QString mOriginal;
    QString mTool;
    std::unique_ptr<PresetSampler> mSampler;
    QImage mPreview;
    QLabel* mPreviewLabel = nullptr;
    QLabel* mMessage = nullptr;
    QLineEdit* mName = nullptr;
    QLineEdit* mDescription = nullptr;
    QLineEdit* mTip = nullptr;
    QCheckBox* mGlobalCurve = nullptr;
    PressureCurveWidget* mCurve = nullptr;
};

#endif // BRUSHPRESETEDITOR_H
