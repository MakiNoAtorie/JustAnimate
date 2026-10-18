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
#include "object.h"
#include "appinfo.h"

#include <algorithm>
#include <cmath>

#include <QDomDocument>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>

#include "layer.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layersound.h"
#include "layercamera.h"

#include "util.h"
#include "bitmapimage.h"
#include "vectorimage.h"
#include "fileformat.h"
#include "activeframepool.h"
#include "framerenderer.h"


Object::Object()
{
    mActiveFramePool.reset(new ActiveFramePool);
}

Object::~Object()
{
    mActiveFramePool->clear();

    for (Layer* layer : mLayers)
        delete layer;
    mLayers.clear();

    deleteWorkingDir();
}

void Object::init()
{
    createWorkingDir();

    // default palette
    loadDefaultPalette();
}

QDomElement Object::saveXML(QDomDocument& doc) const
{
    QDomElement objectTag = doc.createElement("object");
    if (mSceneLength > 0) { objectTag.setAttribute("length", mSceneLength); }

    for (Layer* layer : mLayers)
    {
        QDomElement layerTag = layer->createDomElement(doc);
        objectTag.appendChild(layerTag);
    }
    // Pencil2D reads only <layer> children and skips these.
    for (const LayerGroup& group : mLayerGroups)
    {
        if (layerGroupRange(group.id).first < 0) { continue; } // emptied by a layer delete
        QDomElement groupTag = doc.createElement("layerGroup");
        groupTag.setAttribute("id", group.id);
        groupTag.setAttribute("name", group.name);
        if (group.collapsed) { groupTag.setAttribute("collapsed", 1); }
        if (!group.visible) { groupTag.setAttribute("visibility", 0); }
        if (group.opacity != 1.0) { groupTag.setAttribute("opacity", group.opacity); }
        objectTag.appendChild(groupTag);
    }
    return objectTag;
}

bool Object::loadXML(const QDomElement& docElem, ProgressCallback progressForward)
{
    if (docElem.isNull())
    {
        return false;
    }

    const QString dataDirPath = mDataDirPath;
    const int length = docElem.attribute("length").toInt();
    mSceneLength = length > 0 && length <= MaxFramesBound ? length : 0;

    for (QDomNode node = docElem.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement(); // try to convert the node to an element.
        if (element.tagName() == "layerGroup")
        {
            LayerGroup group;
            group.id = element.attribute("id").toInt();
            group.name = element.attribute("name");
            group.collapsed = element.attribute("collapsed", "0").toInt();
            group.visible = element.attribute("visibility", "1").toInt();
            group.opacity = qBound(0.0, element.attribute("opacity", "1").toDouble(), 1.0);
            if (group.id > 0 && findLayerGroup(group.id) == nullptr) { mLayerGroups.append(group); }
            continue;
        }
        if (element.tagName() != "layer")
        {
            continue;
        }

        Layer* newLayer;
        switch (element.attribute("type").toInt())
        {
        case Layer::BITMAP:
            newLayer = new LayerBitmap(getUniqueLayerID());
            break;
        case Layer::VECTOR:
            newLayer = new LayerVector(getUniqueLayerID());
            break;
        case Layer::SOUND:
            newLayer = new LayerSound(getUniqueLayerID());
            break;
        case Layer::CAMERA:
            newLayer = new LayerCamera(getUniqueLayerID());
            break;
        default:
            qWarning() << "Skipping layer with unknown type" << element.attribute("type");
            continue;
        }
        mLayers.append(newLayer);
        newLayer->loadDomElement(element, dataDirPath, progressForward);
    }
    // Untrusted file: group members must be contiguous, so a member after its group's first run loads ungrouped.
    QSet<int> endedGroups;
    int previous = 0;
    for (Layer* layer : mLayers)
    {
        if (findLayerGroup(layer->groupId()) == nullptr) { layer->setGroupId(0); }
        if (layer->groupId() != previous && previous != 0) { endedGroups.insert(previous); }
        if (endedGroups.contains(layer->groupId())) { layer->setGroupId(0); }
        previous = layer->groupId();
    }
    return true;
}

LayerGroup* Object::findLayerGroup(int groupId)
{
    return const_cast<LayerGroup*>(static_cast<const Object*>(this)->findLayerGroup(groupId));
}

const LayerGroup* Object::findLayerGroup(int groupId) const
{
    for (const LayerGroup& group : mLayerGroups)
    {
        if (group.id == groupId) { return &group; }
    }
    return nullptr;
}

int Object::getUniqueLayerGroupID() const
{
    int id = 1;
    for (const LayerGroup& group : mLayerGroups) { id = std::max(id, group.id + 1); }
    return id;
}

QPair<int, int> Object::layerGroupRange(int groupId) const
{
    QPair<int, int> range(-1, -1);
    for (int i = 0; groupId != 0 && i < mLayers.size(); ++i)
    {
        if (mLayers[i]->groupId() != groupId) { continue; }
        if (range.first < 0) { range.first = i; }
        range.second = i;
    }
    return range;
}

TimesheetColumn effectiveTimesheetColumn(const Object& object, int layerIndex)
{
    const Layer* layer = object.getLayer(layerIndex);
    if (layer == nullptr) { return TimesheetColumn::Default; }
    if (layer->type() == Layer::SOUND) { return TimesheetColumn::Dialogue; }
    if (layer->timesheetColumn() != TimesheetColumn::Default) { return layer->timesheetColumn(); }
    if (!layer->isPaintable()) { return TimesheetColumn::Camera; }
    int below = 0;
    for (int i = 0; i < layerIndex; ++i) { below += object.getLayer(i)->isPaintable() ? 1 : 0; }
    const int key = static_cast<int>(TimesheetColumn::KeyA);
    const int inbetween = static_cast<int>(TimesheetColumn::InbetweenA);
    return static_cast<TimesheetColumn>(below < 6 ? key + below : inbetween + qMin(below - 6, 5));
}

bool Object::isLayerFolded(int index) const
{
    const Layer* layer = getLayer(index);
    if (layer == nullptr) { return false; }
    const LayerGroup* group = findLayerGroup(layer->groupId());
    return group && group->collapsed && layerGroupRange(group->id).second != index;
}

bool Object::isLayerVisible(const Layer* layer) const
{
    const LayerGroup* group = findLayerGroup(layer->groupId());
    return layer->visible() && (group == nullptr || group->visible);
}

LayerArrangement Object::layerArrangement() const
{
    LayerArrangement a;
    for (const Layer* layer : mLayers)
    {
        a.layerIds << layer->id();
        a.groupIds << layer->groupId();
    }
    a.groups = mLayerGroups;
    return a;
}

void Object::setLayerArrangement(const LayerArrangement& a)
{
    QList<Layer*> ordered;
    for (int i = 0; i < a.layerIds.size(); ++i)
    {
        Layer* layer = findLayerById(a.layerIds[i]);
        if (layer == nullptr) { continue; }
        layer->setGroupId(a.groupIds[i]);
        ordered << layer;
    }
    for (Layer* layer : mLayers)
    {
        if (!ordered.contains(layer)) { ordered << layer; }
    }
    mLayers = ordered;
    mLayerGroups = a.groups;
}

LayerBitmap* Object::addNewBitmapLayer()
{
    LayerBitmap* layerBitmap = new LayerBitmap(getUniqueLayerID());
    mLayers.append(layerBitmap);

    layerBitmap->addNewKeyFrameAt(1);

    return layerBitmap;
}

LayerVector* Object::addNewVectorLayer()
{
    LayerVector* layerVector = new LayerVector(getUniqueLayerID());
    mLayers.append(layerVector);

    layerVector->addNewKeyFrameAt(1);

    return layerVector;
}

LayerSound* Object::addNewSoundLayer()
{
    LayerSound* layerSound = new LayerSound(getUniqueLayerID());
    mLayers.append(layerSound);

    // No default keyFrame at position 1 for Sound layer.

    return layerSound;
}

LayerCamera* Object::addNewCameraLayer()
{
    LayerCamera* layerCamera = new LayerCamera(getUniqueLayerID());
    mLayers.append(layerCamera);

    layerCamera->addNewKeyFrameAt(1);

    return layerCamera;
}

void Object::createWorkingDir()
{
    QString projectName;
    if (mFilePath.isEmpty())
    {
        projectName = "Default";
    }
    else
    {
        QFileInfo fileInfo(mFilePath);
        projectName = fileInfo.completeBaseName();
    }
    const QString tempRoot = QString("%1/%2/").arg(QDir::tempPath(), APP_TEMP_DIR_NAME);
    QDir dir;
    dir.mkpath(tempRoot);

    // mkdir fails on an existing folder, so two processes never share one.
    QString strWorkingDir;
    int attempts = 0;
    do
    {
        strWorkingDir = QString("%1%2_%3_%4/").arg(tempRoot, projectName, PFF_TMP_DECOMPRESS_EXT, uniqueString(8));
    }
    while (!dir.mkdir(strWorkingDir) && ++attempts < 100);

    mWorkingDirPath = strWorkingDir;

    QDir dataDir(strWorkingDir + PFF_DATA_DIR);
    dataDir.mkpath(".");

    mDataDirPath = dataDir.absolutePath();
}

void Object::deleteWorkingDir() const
{
    if (!mWorkingDirPath.isEmpty())
    {
        QDir dir(mWorkingDirPath);
        // Another process can hold a file open here; a temp folder left behind is harmless.
        if (!dir.removeRecursively())
            qWarning() << "Could not remove working folder" << mWorkingDirPath;
    }
}

void Object::setWorkingDir(const QString& path)
{
    QDir dir(path);
    Q_ASSERT(dir.exists());
    mWorkingDirPath = path;
}

int Object::getMaxLayerID()
{
    int maxId = 0;
    for (Layer* iLayer : mLayers)
    {
        if (iLayer->id() > maxId)
        {
            maxId = iLayer->id();
        }
    }
    return maxId;
}

int Object::getUniqueLayerID()
{
    return 1 + getMaxLayerID();
}

Layer* Object::getLayer(int i) const
{
    if (i < 0 || i >= getLayerCount())
    {
        return nullptr;
    }

    return mLayers.at(i);
}

Layer* Object::getLayerBelow(int i, Layer::LAYER_TYPE type) const
{
    for (; i >= 0; --i)
    {
        Layer* layerCheck = getLayer(i);
        Q_ASSERT(layerCheck);
        if (layerCheck->type() == type)
        {
            return layerCheck;
        }
    }

    return nullptr;
}

Layer* Object::findLayerById(int layerId) const
{
    for(Layer* layer : mLayers)
    {
        if (layer->id() == layerId)
        {
            return layer;
        }
    }
    return nullptr;
}

Layer* Object::findLayerByName(const QString& strName, Layer::LAYER_TYPE type) const
{
    bool bCheckType = (type != Layer::UNDEFINED);
    for (Layer* layer : mLayers)
    {
        bool isTypeMatch = (bCheckType) ? (type == layer->type()) : true;
        if (isTypeMatch && layer->name() == strName)
        {
            return layer;
        }
    }
    return nullptr;
}

Layer* Object::takeLayer(int layerId)
{
    // Removes the layer from this Object and returns it
    // The ownership of this layer has been transfer to the caller
    int index = -1;
    for (int i = 0; i< mLayers.length(); ++i)
    {
        Layer* layer = mLayers[i];
        if (layer->id() == layerId)
        {
            index = i;
            break;
        }
    }

    if (index == -1) { return nullptr; }

    Layer* layer = mLayers.takeAt(index);
    return layer;
}

void Object::insertLayer(int index, Layer* layer)
{
    mLayers.insert(qBound(0, index, static_cast<int>(mLayers.size())), layer);
}

bool Object::canSwapLayers(int layerIndexLeft, int layerIndexRight) const
{
    if (layerIndexLeft < 0 || layerIndexLeft >= mLayers.size())
    {
        return false;
    }

    if (layerIndexRight < 0 || layerIndexRight >= mLayers.size())
    {
        return false;
    }

    Layer* firstLayer = mLayers.first();
    Layer* leftLayer = mLayers.at(layerIndexLeft);
    Layer* rightLayer = mLayers.at(layerIndexRight);

    // The bottom layer can't be swapped!
    if ((leftLayer->type() == Layer::CAMERA ||
         rightLayer->type() == Layer::CAMERA) &&
         (firstLayer == leftLayer || firstLayer == rightLayer)) {
        return false;
    }
    return true;
}

bool Object::canDeleteLayer(int index) const
{
    // The first camera layer sits at the bottom and is the last camera in a new project.
    if (index == 0) {
        return false;
    }

    if (mLayers.at(index) == nullptr)
    {
        return false;
    }
    if (mLayers.at(index)->type() == Layer::CAMERA && getLayersByType<LayerCamera>().size() == 1)
    {
        return false;
    }

    return true;
}

void Object::deleteLayer(int i)
{
    if (i > -1 && i < mLayers.size())
    {
        delete mLayers.takeAt(i);
    }
}

void Object::deleteLayer(Layer* layer)
{
    auto it = std::find(mLayers.begin(), mLayers.end(), layer);

    if (it != mLayers.end())
    {
        delete layer;
        mLayers.erase(it);
    }
}

bool Object::addLayer(Layer* layer)
{
    if (layer == nullptr || mLayers.contains(layer))
    {
        return false;
    }
    layer->setId(getUniqueLayerID());
    layer->setGroupId(0); // the group belongs to the object the layer came from
    mLayers.append(layer);
    return true;
}

ColorRef Object::getColor(int index) const
{
    ColorRef result(Qt::white, tr("error"));
    if (index > -1 && index < mPalette.size())
    {
        result = mPalette.at(index);
    }
    return result;
}

void Object::setColor(int index, const QColor& newColor)
{
    Q_ASSERT(index >= 0);

    mPalette[index].color = newColor;
}

void Object::setColorRef(int index, const ColorRef& newColorRef)
{
    mPalette[index] = newColorRef;
}

void Object::movePaletteColor(int start, int end)
{
    mPalette.move(start, end);
}

void Object::moveVectorColor(int start, int end)
{
    for (Layer* layer : mLayers)
    {
        if (layer->type() == Layer::VECTOR)
        {
            static_cast<LayerVector*>(layer)->moveColor(start, end);
        }
    }
}

void Object::importVectorColors(LayerVector* layer, const Object& source, QMap<int, int>& importedColors)
{
    QMap<int, int> mapping;
    for (int i = 0; i < source.getColorCount(); i++)
    {
        if (!layer->usesColor(i))
        {
            continue;
        }
        if (!importedColors.contains(i))
        {
            addColor(source.getColor(i));
            importedColors[i] = getColorCount() - 1;
        }
        mapping[i] = importedColors[i];
    }
    layer->remapColors(mapping);
}

void Object::addColorAtIndex(int index, const ColorRef& newColor)
{
    mPalette.insert(index, newColor);
}

bool Object::isColorInUse(int index) const
{
    for (Layer* layer : mLayers)
    {
        if (layer->type() == Layer::VECTOR)
        {
            LayerVector* layerVector = static_cast<LayerVector*>(layer);

            if (layerVector->usesColor(index))
            {
                return true;
            }
        }
    }
    return false;
}

void Object::removeColor(int index)
{
    for (Layer* layer : mLayers)
    {
        if (layer->type() == Layer::VECTOR)
        {
            LayerVector* layerVector = static_cast<LayerVector*>(layer);
            layerVector->removeColor(index);
        }
    }

    mPalette.removeAt(index);

    // update the vector pictures using that color!
}

void Object::renameColor(int i, const QString& text)
{
    mPalette[i].name = text;
}

QString Object::savePalette(const QString& dataFolder) const
{
    QString fullPath = QDir(dataFolder).filePath(PFF_PALETTE_FILE);
    bool ok = exportPalette(fullPath);
    if (ok)
        return fullPath;
    return "";
}

void Object::exportPaletteGPL(QFile& file) const
{
    QString fileName = QFileInfo(file).baseName();
    QTextStream out(&file);

    out << "GIMP Palette" << "\n";
    out << "Name: " << fileName << "\n";
    out << "#" << "\n";

    for (const ColorRef& ref : mPalette)
    {
        QColor toRgb = ref.color.toRgb();
        out << QString("%1 %2 %3").arg(toRgb.red()).arg(toRgb.green()).arg(toRgb.blue());
        out << " " << ref.name << "\n";
    }
}

void Object::exportPalettePencil(QFile& file) const
{
    QTextStream out(&file);

    QDomDocument doc("PencilPalette");
    QDomElement root = doc.createElement("palette");
    doc.appendChild(root);
    for (const ColorRef& ref : mPalette)
    {
        QDomElement tag = doc.createElement("Color");
        tag.setAttribute("name", ref.name);
        tag.setAttribute("red", ref.color.red());
        tag.setAttribute("green", ref.color.green());
        tag.setAttribute("blue", ref.color.blue());
        tag.setAttribute("alpha", ref.color.alpha());
        root.appendChild(tag);
    }
    int indentSize = 2;
    doc.save(out, indentSize);
}

bool Object::exportPalette(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        qDebug("Error: cannot export palette");
        return false;
    }

    if (file.fileName().endsWith(".gpl", Qt::CaseInsensitive))
        exportPaletteGPL(file);
    else
        exportPalettePencil(file);

    return true;
}

/* Import the .gpl GIMP palette format.
 *
 * This functions supports importing both the old and new .gpl formats.
 * This should load colors the same as GIMP, with the following intentional exceptions:
 * - Whitespace before and after a name does not appear in the name
 * - The last line is processed, even if there is not a trailing newline
 * - Colors without a name will use our automatic naming system rather than "Untitled"
 */
void Object::importPaletteGPL(QFile& file)
{
    QTextStream in(&file);
    QString line;

    // The first line must start with "GIMP Palette"
    // Displaying an error here would be nice
    in.readLineInto(&line);
    if (!line.startsWith("GIMP Palette")) return;

    in.readLineInto(&line);

    // There are two GPL formats, the new one must start with "Name: " on the second line
    if (line.startsWith("Name: "))
    {
        in.readLineInto(&line);
        // The new format contains an optional third line starting with "Columns: "
        if (line.startsWith("Columns: "))
        {
            // Skip to the next line
            in.readLineInto(&line);
        }
    }

    // Colors inherit the value from the previous color for missing channels
    // Some palettes may rely on this behavior, so we should try to replicate it
    QColor prevColor(Qt::black);

    do
    {
        // Ignore comments and empty lines
        if (line.isEmpty() || line.startsWith("#")) continue;

        int red = 0;
        int green = 0;
        int blue = 0;

        int countInLine = 0;
        QString name = "";
        for(const QString& snip : line.split(QRegularExpression("\\s|\\t"), Qt::SkipEmptyParts))
        {
            switch (countInLine)
            {
            case 0:
                red = snip.toInt();
                break;
            case 1:
                green = snip.toInt();
                break;
            case 2:
                blue = snip.toInt();
                break;
            default:
                name += snip + " ";
            }
            countInLine++;
        }

        // trim additional spaces
        name = name.trimmed();

        // Get values from previous color if necessary
        if (countInLine < 2) green = prevColor.green();
        if (countInLine < 3) blue = prevColor.blue();

        // GIMP assigns colors the name "Untitled" by default now,
        // so in addition to missing names, we also use automatic
        // naming for this
        if (name.isEmpty() || name == "Untitled") name = QString();

        QColor color(red, green, blue);
        if (color.isValid())
        {
            mPalette.append(ColorRef(color, name));
            prevColor = color;
        }
    } while (in.readLineInto(&line));
}

void Object::importPalettePencil(QFile& file)
{
    QDomDocument doc;
    doc.setContent(&file);

    QDomElement docElem = doc.documentElement();
    QDomNode tag = docElem.firstChild();
    while (!tag.isNull())
    {
        QDomElement e = tag.toElement(); // try to convert the node to an element.
        if (!e.isNull())
        {
            QString name = e.attribute("name");
            int r = e.attribute("red").toInt();
            int g = e.attribute("green").toInt();
            int b = e.attribute("blue").toInt();
            int a = e.attribute("alpha", "255").toInt();
            mPalette.append(ColorRef(QColor(r, g, b, a), name));
        }
        tag = tag.nextSibling();
    }
}

void Object::openPalette(const QString& filePath)
{
    if (!QFile::exists(filePath))
    {
        return;
    }

    mPalette.clear();
    importPalette(filePath);
}

/*
 * Imports palette, e.g. appends to palette
*/
bool Object::importPalette(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::ReadOnly))
    {
        return false;
    }

    if (file.fileName().endsWith(".gpl", Qt::CaseInsensitive))
    {
        importPaletteGPL(file);
    } else {
        importPalettePencil(file);
    }
    return true;
}


namespace
{
struct BuiltinColor { QColor color; const char* name; };
struct BuiltinPalette { const char* name; QList<BuiltinColor> colors; };

// One table for every built-in palette; the first is the Default palette.
const QList<BuiltinPalette>& builtinPalettes()
{
    static const QList<BuiltinPalette> table = {
        { QT_TRANSLATE_NOOP("Object", "Default"), {
            { QColor(Qt::black), QT_TRANSLATE_NOOP("Object", "Black") },
            { QColor(Qt::red), QT_TRANSLATE_NOOP("Object", "Red") },
            { QColor(Qt::darkRed), QT_TRANSLATE_NOOP("Object", "Dark Red") },
            { QColor(255, 128, 0), QT_TRANSLATE_NOOP("Object", "Orange") },
            { QColor(128, 64, 0), QT_TRANSLATE_NOOP("Object", "Dark Orange") },
            { QColor(Qt::yellow), QT_TRANSLATE_NOOP("Object", "Yellow") },
            { QColor(Qt::darkYellow), QT_TRANSLATE_NOOP("Object", "Dark Yellow") },
            { QColor(Qt::green), QT_TRANSLATE_NOOP("Object", "Green") },
            { QColor(Qt::darkGreen), QT_TRANSLATE_NOOP("Object", "Dark Green") },
            { QColor(Qt::cyan), QT_TRANSLATE_NOOP("Object", "Cyan") },
            { QColor(Qt::darkCyan), QT_TRANSLATE_NOOP("Object", "Dark Cyan") },
            { QColor(Qt::blue), QT_TRANSLATE_NOOP("Object", "Blue") },
            { QColor(Qt::darkBlue), QT_TRANSLATE_NOOP("Object", "Dark Blue") },
            { QColor(255, 255, 255), QT_TRANSLATE_NOOP("Object", "White") },
            { QColor(220, 220, 229), QT_TRANSLATE_NOOP("Object", "Very Light Grey") },
            { QColor(Qt::lightGray), QT_TRANSLATE_NOOP("Object", "Light Grey") },
            { QColor(Qt::gray), QT_TRANSLATE_NOOP("Object", "Grey") },
            { QColor(Qt::darkGray), QT_TRANSLATE_NOOP("Object", "Dark Grey") },
            { QColor(255, 227, 187), QT_TRANSLATE_NOOP("Object", "Pale Orange Yellow") },
            { QColor(221, 196, 161), QT_TRANSLATE_NOOP("Object", "Pale Grayish Orange Yellow") },
            { QColor(255, 214, 156), QT_TRANSLATE_NOOP("Object", "Orange Yellow ") },
            { QColor(207, 174, 127), QT_TRANSLATE_NOOP("Object", "Grayish Orange Yellow") },
            { QColor(255, 198, 116), QT_TRANSLATE_NOOP("Object", "Light Orange Yellow") },
            { QColor(227, 177, 105), QT_TRANSLATE_NOOP("Object", "Light Grayish Orange Yellow") } } },
        { QT_TRANSLATE_NOOP("Object", "Genga"), {
            { QColor(0, 0, 0), QT_TRANSLATE_NOOP("Object", "Main line") },
            { QColor(220, 0, 0), QT_TRANSLATE_NOOP("Object", "Shadow line") },
            { QColor(0, 90, 255), QT_TRANSLATE_NOOP("Object", "Highlight line") },
            { QColor(0, 160, 0), QT_TRANSLATE_NOOP("Object", "Second shadow line") } } },
    };
    return table;
}
}

int Object::builtinPaletteCount()
{
    return builtinPalettes().size();
}

QString Object::builtinPaletteName(int index)
{
    return tr(builtinPalettes().at(index).name);
}

void Object::loadDefaultPalette()
{
    mPalette.clear();
    for (const BuiltinColor& c : builtinPalettes().at(0).colors)
    {
        addColor(ColorRef(c.color, tr(c.name)));
    }
}

void Object::loadBuiltinPalette(int index)
{
    mPalette.clear();
    for (const BuiltinColor& c : builtinPalettes().at(index).colors)
    {
        addColor(ColorRef(c.color, tr(c.name)));
    }
    modification();
}
bool Object::isPaletteInUse() const
{
    for (int i = 0; i < getColorCount(); i++)
    {
        if (isColorInUse(i)) { return true; }
    }
    return false;
}

QString Object::copyFileToDataFolder(const QString& strFilePath)
{
    if (!QFile::exists(strFilePath))
    {
        qDebug() << "[Object] sound file doesn't exist: " << strFilePath;
        return "";
    }

    QString sNewFileName = "sound_";
    sNewFileName += QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz.");
    sNewFileName += QFileInfo(strFilePath).suffix();

    QString destFile = QDir(mDataDirPath).filePath(sNewFileName);

    if (QFile::exists(destFile))
    {
        QFile::remove(destFile);
    }

    bool bCopyOK = QFile::copy(strFilePath, destFile);
    if (!bCopyOK)
    {
        qDebug() << "[Object] couldn't copy sound file to data folder: " << strFilePath;
        return "";
    }

    return destFile;
}

Object::SpriteSheetGrid Object::spriteSheetGrid(int frameCount, int columns)
{
    SpriteSheetGrid grid;
    const int n = std::max(frameCount, 1);
    grid.columns = columns > 0 ? std::min(columns, n) : static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    grid.rows = (n + grid.columns - 1) / grid.columns;
    return grid;
}

int Object::getLayerCount() const
{
    return mLayers.size();
}

void Object::setData(const ObjectData& d)
{
    mData = d;
}

int Object::totalKeyFrameCount() const
{
    int sum = 0;
    for (const Layer* layer : mLayers)
    {
        sum += layer->keyFrameCount();
    }
    return sum;
}

void Object::updateActiveFrames(int frame) const
{
    const int beginFrame = std::max(frame - 3, 1);
    const int endFrame = frame + 4;

    const int minFrameCount = getLayerCount() * (endFrame - beginFrame);
    mActiveFramePool->setMinFrameCount(minFrameCount);
    // Frames this far away reload from disk on demand; the budget alone keeps whole projects in RAM.
    mActiveFramePool->unloadFramesFarFrom(frame, 50);

    for (Layer* layer : mLayers)
    {
        layer->setFramePool(mActiveFramePool.get());
        if (!layer->visible()) { continue; }
        // Only the shown Drawing blocks; the neighbours load on worker threads.
        mActiveFramePool->put(layer->getLastKeyFrameAtPosition(frame), true);
        for (int k = beginFrame; k < endFrame; ++k)
        {
            mActiveFramePool->put(layer->getKeyFrameAt(k), false);
        }
    }
}

void Object::preloadFrames(int frame, int count) const
{
    for (Layer* layer : mLayers)
    {
        if (layer->type() != Layer::BITMAP || !layer->visible()) { continue; }
        for (int k = frame; k < frame + count; ++k)
        {
            // Does not block: the frame takes the result when it is drawn or enters the pool.
            // A loaded frame is skipped, so an edit is never replaced by the file on disk.
            mActiveFramePool->put(layer->getKeyFrameAt(k), false);
        }
    }
}

quint64 Object::framePoolMemory() const
{
    return mActiveFramePool->usedMemory();
}

void Object::setActiveFramePoolSize(int sizeInMB)
{
    // convert MB to Byte
    mActiveFramePool->resize(qint64(sizeInMB) * 1024 * 1024);
}
