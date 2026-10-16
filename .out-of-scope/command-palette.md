# Command palette

JustAnimate has no command palette (a Ctrl+Shift+P searchable list of every action).

## Why this is out of scope

The roadmap's lean rules build a feature only when someone needs it ("Does it need to exist?"). Every action already has a menu entry and a configurable shortcut in Preferences > Shortcuts, so a palette is a second way to reach the same actions. It would also need its own search UI, ranking and a list of actions that stays in sync with the menus. Nobody has asked for one.

If users ask for it, reopen the idea: the action list can come from the existing `QAction`s that the shortcuts page already lists.

## Prior requests

- #84: "Command palette (4.4)"
