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
- **Double-click** a file to open it with the shell default (**`.modr` → Drawing**, everything else → Text Editor).
- Click **Up** in the toolbar (or use context menu) to go to the parent directory.
- **Arrow Up / Down** moves the primary selection; **Enter** activates it (same as double-click).

### Multi-select

- **Ctrl+click** toggles an item in the selection.
- **Shift+click** or **Shift+Up/Down** selects a range from the anchor.
- **Ctrl+A** selects all entries in the current folder.
- Primary selection is drawn slightly brighter than other selected rows.
- Delete can apply to the whole multi-selection (with confirmation). Copy/Cut/Rename still require a single item.

### Properties

- **Space** or context menu **Properties** shows name, virtual path, type, and size (files) or child count (folders) in the status bar.
- Multi-select Properties summarizes counts.

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
| Up / Down | Move selection (Shift extends range) |
| Enter | Open directory or file; or confirm pending delete |
| Backspace | Go up one directory |
| Delete | Request delete (second press confirms; multi-select OK) |
| Space | Properties for selection |
| Ctrl+A | Select all in folder |
| F2 | Start rename on selected entry (single item) |
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

- Open (default app for type)
- Open with Text Editor / Open with Drawing
- Properties
- Copy / Cut / Paste
- Rename
- Delete

**Right-click a directory:**

- Open (enter directory)
- Properties
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

- Default open supports text + `.modr` only; force open-with can open any file in Editor or Drawing (Drawing rejects non-`.modr` loads).
- No drag-and-drop.
- Copy/Cut of multi-select is not supported yet.
- Clipboard is per browser window (not shared across Filesystem instances or the host OS).

## Developer Notes

Main implementation files:

- `src/app/FilesystemApp.hpp`
- `src/app/FilesystemApp.cpp`
- `src/fs/Filesystem.*` — shared recursive copy/remove, path helpers, `fileSize`
- `src/window/WindowManager.cpp` — `launchFilesystem()`, `openPath` / open-with shell bridges

File open uses `IWindowController` (`openPath`, `openInTextEditor`, `openInDrawing`) so the browser does not depend on Editor or Drawing classes.