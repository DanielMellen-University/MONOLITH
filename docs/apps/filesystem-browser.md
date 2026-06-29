# Filesystem Browser App

The Filesystem Browser is a graphical file manager for Monolith's [internal filesystem](../filesystem.md). It provides directory navigation, file creation, renaming, deletion, and opening text files in the editor.

## Launching

Open **Filesystem** from the Start menu. Multiple instances are supported:

- `Filesystem`
- `Filesystem 2`
- `Filesystem 3`

## Layout

The window has four regions:

1. **Path bar** — shows the current virtual directory
2. **Toolbar** — Up, New Folder, New File, Delete, Rename
3. **List view** — directories (`▶`) and files (`•`), sorted with directories first
4. **Status bar** — feedback messages for actions

## Navigation

- **Double-click** a directory to enter it.
- **Double-click** a text file to open it in the Text Editor.
- **Double-click** a `.modr` file to open it in Drawing.
- Click **Up** in the toolbar (or use context menu) to go to the parent directory.
- **Arrow Up / Down** moves selection; **Enter** activates the selected entry (same as double-click).

The browser starts at `/home/monolith` when that path exists.

## Toolbar Actions

| Button | Action |
|--------|--------|
| Up | Navigate to parent directory |
| New Folder | Create `New Folder` (auto-increments if name exists) |
| New File | Create `New File.txt` (auto-increments if name exists) |
| Delete | Delete selected entry (confirmation required) |
| Rename | Rename selected entry (inline edit) |

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Up / Down | Move selection |
| Enter | Open directory or file |
| Backspace | Go up one directory |
| Delete | Delete selected entry |
| F2 | Start rename on selected entry |
| F5 | Refresh directory listing |
| Esc | Cancel rename or close context menu |

### Rename Mode

Press **F2** or choose Rename from the context menu. Type the new name, then **Enter** to commit or **Esc** to cancel.

### Delete Confirmation

Delete (toolbar, keyboard, or context menu) shows a confirmation submenu: **Confirm Delete** or **Cancel**.

## Context Menus

**Right-click empty space:**

- New Folder
- New File
- Refresh

**Right-click a file:**

- Open (opens in Text Editor, or Drawing for `.modr` files)
- Rename
- Delete

**Right-click a directory:**

- Open (enter directory)
- Rename
- Delete

## Current Limitations

- Only text files and `.modr` drawings open from here (no generic “open with” yet).
- No copy, move, or drag-and-drop.
- No multi-select.
- No file preview or properties panel.
- Recursive delete is not exposed (use Terminal `rm -r` for directory trees if needed).

## Developer Notes

Main implementation files:

- `src/app/FilesystemApp.hpp`
- `src/app/FilesystemApp.cpp`
- `src/window/WindowManager.cpp` — `launchFilesystem()`, `openInTextEditor()`, `openInDrawing()` shell bridges

File open uses `IWindowController` shell methods so the browser does not depend directly on Text Editor or Drawing classes.