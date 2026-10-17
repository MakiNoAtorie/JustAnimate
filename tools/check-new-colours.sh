#!/bin/bash
# Fails when a line added since <base> hard-codes a UI colour. Usage: tools/check-new-colours.sh <base ref>
# Drawing data (palettes, paper, pixels) keeps its literal colour with a "palette-exempt:" comment on that line.
base=${1:-origin/master}
hits=$(git diff -U0 "$base"...HEAD -- 'app/src' 'core_lib/src' ':!core_lib/src/external' \
    | grep -E '^\+[^+]' \
    | grep -E 'QColor\([0-9]|Qt::(white|black)\b|"#[0-9A-Fa-f]{6}' \
    | grep -v 'palette-exempt:')
if [ -n "$hits" ]; then
    echo "Hard-coded colour: take it from the palette (QPalette role, Theme), or mark drawing data with // palette-exempt: <why>"
    echo "$hits"
    exit 1
fi
