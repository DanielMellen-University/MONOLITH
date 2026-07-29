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
| Delete | Request delete of selected entry (files or folder trees; confirmation required) |
| Rename | Rename selected entry (inline edit) |

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Up / Down | Move selection |
| Enter | Open directory or file; or confirm pending delete |
| Backspace | Go up one directory |
| Delete | Request delete (second press confirms) |
| F2 | Start rename on selected entry |
| F5 | Refresh directory listing |
| Esc | Cancel rename, cancel pending delete, or close context menu |

### Rename Mode

Press **F2** or choose Rename from the context menu. Type the new name, then **Enter** to commit or **Esc** to cancel.

### Delete Confirmation

Deleting always requires confirmation (files and non-empty folders):

1. **Toolbar Delete** or **Delete** key: status bar asks to confirm; press **Delete** / **Enter** again to remove, or **Esc** to cancel. Changing selection cancels the pending delete.
2. **Context menu Delete**: submenu with **Confirm Delete** / **Cancel**.

Deletion uses `Filesystem::removeRecursive` (whole directory trees). The virtual root `/` cannot be deleted.

## Context Menus

**Right-click empty space:**

- New Folder
- New File
- Refresh

**Right-click a file:**

- Open (opens in Text Editor, or Drawing for `.modr` files)
- Copy / Cut / Paste
- Rename
- Delete

**Right-click a directory:**

- Open (enter directory)
- Copy / Cut / Paste
- Rename
- Delete

**Right-click empty space:**

- New Folder / New File
- Paste (when something is on the clipboard)
- Refresh

## Copy, Cut, and Paste

- **Copy** or **Cut** a selected file or folder from the right-click menu (or Ctrl+C / Ctrl+X).
- **Paste** into the current directory from the right-click menu (or Ctrl+V).
- Cut + Paste moves items; Copy + Paste duplicates them (including directory trees via `Filesystem::copyRecursive` / `removeRecursive`).
- Paste is blocked if the destination already contains an item with the same name, or if you try to paste a folder into itself (`isSameOrDescendant`).

## Current Limitations

- Only text files and `.modr` drawings open from here (no generic “open with” yet).
- No drag-and-drop.
- No multi-select.
- No file preview or properties panel.
- Clipboard is per browser window (not shared across Filesystem instances or the host OS).

## Developer Notes

Main implementation files:

- `src/app/FilesystemApp.hpp`
- `src/app/FilesystemApp.cpp`
- `src/fs/Filesystem.*` — shared recursive copy/remove and path helpers
- `src/window/WindowManager.cpp` — `launchFilesystem()`, `openInTextEditor()`, `openInDrawing()` shell bridges

File open uses `IWindowController` shell methods so the browser does not depend directly on Text Editor or Drawing classes.