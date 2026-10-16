# Translations

Source language: English, written in `tr("...")` calls and `.ui` files under
`app/` and `core_lib/`. `translations/pencil_<lang>.ts` hold one catalogue per
language (`pencil_en.ts` is the English reference). Strings are not in the
`.ts` files by hand: lupdate extracts them.

## Who runs lupdate, and when

The maintainer, before each release and after a batch of UI-string changes.
Feature commits do not touch `translations/`.

```bash
export PATH="$QTDIR/bin:$PATH"          # Windows: %QTDIR%\bin
lupdate app/src app/ui core_lib/src -ts translations/*.ts
```

- Keep obsolete entries (do not pass `-no-obsolete`): translators can recover
  them when a string returns.
- `pencil_kab.ts` and `pencil_yue.ts` use language codes lupdate does not
  know and skips. Set line 3 `language=` to a code with the same plural count
  (`fr`, `zh_TW`) for the run, then set it back. Do not edit them otherwise.
- Never hand-edit translated text with a script. Commit the lupdate result
  as one commit.

## How translators contribute

Pull request that edits one `translations/pencil_<lang>.ts` (Qt Linguist or a
text editor). Change only `<translation>` text and `type="unfinished"`.
A new language: copy `translations/pencil.ts` to `pencil_<code>.ts`, set
`language=`, add the file to `APP_TRANSLATIONS` in `app/app.cmake`.
Transifex is not used; its config and jobs are removed.

## How `.qm` files are built

`qt_add_translations` in `app/app.cmake` compiles each file in
`APP_TRANSLATIONS` with lrelease during the normal build and embeds the
`.qm` files in the executable under `:/i18n/`. Nothing else to run. A `.ts`
file not listed in `APP_TRANSLATIONS` is not shipped.
