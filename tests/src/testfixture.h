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
#ifndef TESTFIXTURE_H
#define TESTFIXTURE_H

#include <initializer_list>
#include <memory>
#include <QHash>
#include <QSettings>
#include <QVariant>

#include "basetool.h"
#include "editor.h"
#include "object.h"
#include "pencildef.h"
#include "scribblearea.h"

/// Puts the given QSettings keys back as they were (value or absent) at scope exit.
class SettingsGuard
{
public:
    SettingsGuard(std::initializer_list<const char*> keys)
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        for (const char* key : keys) { mSaved.insert(key, settings.value(key)); }
    }
    ~SettingsGuard()
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        for (auto it = mSaved.cbegin(); it != mSaved.cend(); ++it)
        {
            if (it.value().isValid()) { settings.setValue(it.key(), it.value()); }
            else { settings.remove(it.key()); }
        }
    }
    Q_DISABLE_COPY(SettingsGuard)

private:
    QHash<QString, QVariant> mSaved;
};

/// Puts a tool's settings back at scope exit: ~ToolManager saves them to QSettings.
/// Declare it after the editor that owns the tool.
class ToolSettingsGuard
{
public:
    explicit ToolSettingsGuard(BaseTool* tool) : mTool(tool), mSaved(tool->toolProperties()) {}
    ~ToolSettingsGuard() { mTool->toolProperties() = mSaved; }
    Q_DISABLE_COPY(ToolSettingsGuard)

private:
    BaseTool* mTool;
    ToolProperties mSaved;
};

/// An Editor with its ScribbleArea and Object. Add layers to `object`, then call start().
struct TestSession
{
    std::unique_ptr<ScribbleArea> area{ new ScribbleArea(nullptr) };
    std::unique_ptr<Editor> editor{ new Editor }; // destroyed before the area
    Object* object = new Object; // the editor owns it after start()

    TestSession() { object->init(); }
    ~TestSession() { if (!mStarted) { delete object; } }
    Q_DISABLE_COPY(TestSession)

    void start()
    {
        area->setEditor(editor.get());
        editor->setScribbleArea(area.get());
        editor->setObject(object);
        mStarted = true;
        editor->init();
    }

private:
    bool mStarted = false;
};

#endif // TESTFIXTURE_H
