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
#include "papertemplatedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>

PaperTemplateDialog::PaperTemplateDialog(const PaperTemplate& paper, QWidget* parent)
    : QDialog(parent), mPaper(paper)
{
    setWindowTitle(tr("Paper Template"));
    auto* form = new QFormLayout(this);
    mOn = new QCheckBox(tr("Show the genga sheet around the camera frame"), this);
    mOn->setChecked(paper.on);
    form->addRow(mOn);
    auto field = [this, form](const QString& label, const QString& value) {
        auto* edit = new QLineEdit(value, this);
        form->addRow(label, edit);
        return edit;
    };
    mTitle = field(tr("Title:"), paper.title);
    mId = field(tr("ID:"), paper.id);
    mName = field(tr("Name:"), paper.name);
    mState = field(tr("State:"), paper.state);
    mDate = field(tr("Date:"), paper.date);
    mNotes = new QPlainTextEdit(paper.notes, this);
    form->addRow(tr("Notes:"), mNotes);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
}

PaperTemplate PaperTemplateDialog::paperTemplate() const
{
    PaperTemplate paper = mPaper; // keeps the Timesheet values
    paper.on = mOn->isChecked();
    paper.title = mTitle->text();
    paper.id = mId->text();
    paper.name = mName->text();
    paper.state = mState->text();
    paper.date = mDate->text();
    paper.notes = mNotes->toPlainText();
    return paper;
}
