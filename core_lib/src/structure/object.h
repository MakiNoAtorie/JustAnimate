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
#ifndef OBJECT_H
#define OBJECT_H

#include <memory>
#include <QCoreApplication>
#include <QObject>
#include <QList>
#include <QMap>
#include <QColor>
#include <QImage>
#include <QRect>
#include "layer.h"
#include "colorref.h"
#include "pencilerror.h"
#include "pencildef.h"
#include "objectdata.h"

class QFile;
class LayerBitmap;
class LayerVector;
class LayerCamera;
class LayerSound;
class ObjectData;
class ActiveFramePool;

/// A named set of adjacent layers. Members point to it with Layer::groupId. One level, no nesting.
struct LayerGroup
{
    int id = 0;
    QString name;
    bool collapsed = false;
    bool visible = true;
    qreal opacity = 1.0;
    bool operator==(const LayerGroup& o) const
    {
        return id == o.id && name == o.name && collapsed == o.collapsed && visible == o.visible && opacity == o.opacity;
    }
};

/// Layer order (bottom first), the group of each layer and the group list. Undo state of group edits and reorders.
struct LayerArrangement
{
    QList<int> layerIds;
    QList<int> groupIds;
    QList<LayerGroup> groups;
    bool operator==(const LayerArrangement& o) const { return layerIds == o.layerIds && groupIds == o.groupIds && groups == o.groups; }
};

class Object final
{
    Q_DECLARE_TR_FUNCTIONS(Object)
public:
    explicit Object();
    ~Object();

    Object(Object const&) = delete;
    Object(Object&&) = delete;
    Object& operator=(Object const&) = delete;
    Object& operator=(Object&&) = delete;

    void init();
    void createWorkingDir();
    void deleteWorkingDir() const;
    void setWorkingDir(const QString& path); // used by crash recovery

    QString filePath() const { return mFilePath; }
    void    setFilePath(const QString& strFileName) { mFilePath = strFileName; }

    QString workingDir() const { return mWorkingDirPath; }

    QString dataDir() const { return mDataDirPath; }
    void    setDataDir(const QString& dirPath) { mDataDirPath = dirPath; }

    QString mainXMLFile() const { return mMainXMLFile; }
    void    setMainXMLFile(const QString& file) { mMainXMLFile = file; }

    QDomElement saveXML(QDomDocument& doc) const;
    bool loadXML(const QDomElement& element, ProgressCallback progressForward);


    QString copyFileToDataFolder(const QString& strFilePath);

    // Color palette
    ColorRef getColor(int index) const;
    void setColor(int index, const QColor& newColor);
    void setColorRef(int index, const ColorRef& newColorRef);
    void movePaletteColor(int start, int end);
    void moveVectorColor(int start, int end);

    void addColor(const ColorRef& newColor) { mPalette.append(newColor); }
    void addColorAtIndex(int index, const ColorRef& newColor);
    void removeColor(int index);
    void importVectorColors(LayerVector* layer, const Object& source, QMap<int, int>& importedColors);
    bool isColorInUse(int index) const;
    void renameColor(int i, const QString& text);
    int getColorCount() const { return mPalette.size(); }
    bool importPalette(const QString& filePath);
    void importPaletteGPL(QFile& file);
    void importPalettePencil(QFile& file);
    void openPalette(const QString& filePath);

    bool exportPalette(const QString& filePath) const;
    void exportPaletteGPL(QFile& file) const;
    void exportPalettePencil(QFile& file) const;
    QString savePalette(const QString& filePath) const;

    void loadDefaultPalette();
    static int builtinPaletteCount();
    static QString builtinPaletteName(int index);
    /** Replaces the palette with built-in palette index and marks the object modified. */
    void loadBuiltinPalette(int index);
    bool isPaletteInUse() const;

    LayerBitmap* addNewBitmapLayer();
    LayerVector* addNewVectorLayer();
    LayerSound* addNewSoundLayer();
    LayerCamera* addNewCameraLayer();

    int  getLayerCount() const;
    Layer* getLayer(int i) const;
    Layer* getLayerBelow(int i, Layer::LAYER_TYPE type) const;
    Layer* findLayerByName(const QString& strName, Layer::LAYER_TYPE type = Layer::UNDEFINED) const;
    Layer* findLayerById(int layerId) const;
    Layer* takeLayer(int layerId); // Note: transfer ownership of the layer
    void insertLayer(int index, Layer* layer); // Note: takes ownership; the layer keeps its id


    /** Allows you to check whether two layers can be swappped, before doing the actual operation
     *
     *  @param[in] layerIndexLeft The first layer to compare
     *  @param[in] layerIndexRight The second layer to compare
     *
     *  @return true if layers can be swapped, otherwise false
    */
    bool canSwapLayers(int layerIndexLeft, int layerIndexRight) const;

    /** Allows you to check whether the layer at the given index can be deleted
     *
     *  @param[in] index The layer index to check
     *
     *  @return true if the layer can be deleted, otherwise false
    */
    bool canDeleteLayer(int index) const;

    void deleteLayer(int i);
    void deleteLayer(Layer*);
    bool addLayer(Layer* layer);

    const QList<LayerGroup>& layerGroups() const { return mLayerGroups; }
    LayerGroup* findLayerGroup(int groupId);
    const LayerGroup* findLayerGroup(int groupId) const;
    void addLayerGroup(const LayerGroup& group) { mLayerGroups.append(group); }
    int getUniqueLayerGroupID() const;
    /** Lowest and highest layer index of the group's members, or {-1, -1} when it has none */
    QPair<int, int> layerGroupRange(int groupId) const;
    /** True for a member of a collapsed group other than its top member: the timeline shows no row for it */
    bool isLayerFolded(int index) const;
    /** True when the layer and its group, if any, are visible */
    bool isLayerVisible(const Layer* layer) const;
    LayerArrangement layerArrangement() const;
    /** Reorders the listed layers and sets their groups. Unlisted layers keep their order above them. */
    void setLayerArrangement(const LayerArrangement& arrangement);
    /** The saved project length in frames, 0 when unset. LayerManager::animationLength never ends before it. */
    int sceneLength() const { return mSceneLength; }
    void setSceneLength(int length) { mSceneLength = length; }

    template<typename T>
    std::vector<T*> getLayersByType() const
    {
        std::vector<T*> result;
        for (Layer* layer : mLayers)
        {
            T* t = dynamic_cast<T*>(layer);
            if (t)
                result.push_back(t);
        }
        return result;
    }

    // Frames in one PNG: row-major grid in frame order. columns <= 0 means ceil(sqrt(n)).
    struct SpriteSheetGrid
    {
        int columns = 1;
        int rows = 1;
        QRect cell(int index, const QSize& frame) const
        {
            return QRect(QPoint((index % columns) * frame.width(), (index / columns) * frame.height()), frame);
        }
    };
    static SpriteSheetGrid spriteSheetGrid(int frameCount, int columns);

    void modification() { modified = true; }
    bool isModified() const { return modified; }
    void setModified(bool b) { modified = b; }

    int getUniqueLayerID();

    ObjectData* data() { return &mData; }
    const ObjectData* data() const { return &mData; }
    void setData(const ObjectData&);

    int totalKeyFrameCount() const;
    void updateActiveFrames(int frame) const;
    /** Starts reading the bitmap keyframes in [frame, frame + count) of visible layers on worker threads, without blocking */
    void preloadFrames(int frame, int count) const;
    void setActiveFramePoolSize(int sizeInMB);
    /** Bytes of the Drawings the frame pool holds */
    quint64 framePoolMemory() const;

private:
    int getMaxLayerID();

    QString mFilePath;       //< where this object come from. (empty if new project)
    QString mWorkingDirPath; //< the folder that pclx will uncompress to.
    QString mDataDirPath;    //< the folder which contains all bitmap & vector image & sound files.
    QString mMainXMLFile;    //< the location of main.xml

    QList<Layer*> mLayers;
    QList<LayerGroup> mLayerGroups;
    int mSceneLength = 0;
    bool modified = false;

    QList<ColorRef> mPalette;

    ObjectData mData;
    mutable std::unique_ptr<ActiveFramePool> mActiveFramePool;
};

/** The Timesheet column of the layer at `layerIndex`: sound is Dialogue; a chosen column wins; else a camera is
 *  Camera and the n-th drawing layer from the bottom is Keyframe A..F, then In-between A..F (the last holds the rest). */
TimesheetColumn effectiveTimesheetColumn(const Object& object, int layerIndex);


#endif
