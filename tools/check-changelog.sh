#!/usr/bin/env bash
# Guards ChangeLog.md: hand merges and PowerShell rewrites have broken a heading and its UTF-8.
f=ChangeLog.md
rc=0
if LC_ALL=C grep -q $'^\xEF\xBB\xBF' "$f"; then echo "$f: UTF-8 BOM"; rc=1; fi
# Mojibake from a UTF-8 file read as ANSI, or a replacement character.
if grep -n $'\xC3\xA2\xE2\x82\xAC\|\xC3\x83\|\xEF\xBF\xBD' "$f"; then echo "$f: mangled UTF-8; rewrite it as UTF-8"; rc=1; fi
# A heading names a release; an entry with "(#n)" belongs in the list under it.
if grep -n '^## .*(#[0-9]' "$f"; then echo "$f: entry inside a heading"; rc=1; fi
exit $rc
