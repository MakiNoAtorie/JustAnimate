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
#ifndef PAPERTEMPLATEDIALOG_H
#define PAPERTEMPLATEDIALOG_H

#include <QDialog>
#include "objectdata.h"

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;

/// Edits the project's Paper template: on or off and the Title block fields.
class PaperTemplateDialog : public QDialog
{
    Q_OBJECT
public:
    PaperTemplateDialog(const PaperTemplate& paper, QWidget* parent);

    PaperTemplate paperTemplate() const;

private:
    PaperTemplate mPaper;
    QCheckBox* mOn = nullptr;
    QLineEdit* mTitle = nullptr;
    QLineEdit* mId = nullptr;
    QLineEdit* mName = nullptr;
    QLineEdit* mState = nullptr;
    QLineEdit* mDate = nullptr;
    QPlainTextEdit* mNotes = nullptr;
};

#endif // PAPERTEMPLATEDIALOG_H
