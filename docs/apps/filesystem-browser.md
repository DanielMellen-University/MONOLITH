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
2. **Toolbar** — Up, New Folder, New File, Delete, Rename, Filter
3. **List view** — directories (`▶`) and files (`•`), sorted with directories first and names case-insensitively
4. **Status bar** — feedback messages for actions

Long paths and status messages stay at the font's native size and clip at their region's edge. The browser does not horizontally squeeze text to fit a narrow window.

## Navigation

- **Double-click** a directory to enter it.
- **Double-click** a file to open it with the shell default (case-insensitive **`.modr` → Drawing**, everything else → Text Editor).
- Click **Up** in the toolbar or press **Backspace** to go to the parent directory.
- **Arrow Up / Down** moves the primary selection; **Enter** activates it (same as double-click).

### Multi-select

- **Ctrl+click** toggles an item in the selection.
- **Shift+click** or **Shift+Up/Down** selects a range from the anchor.
- **Ctrl+A** selects all entries in the current folder.
- Primary selection is drawn slightly brighter than other selected rows.
- Delete, Copy, and Cut apply to the whole multi-selection. Rename still requires a single item.
- Refreshing or filtering keeps every selected entry that is still visible by name and type; if the primary entry disappears, a surviving selected entry becomes primary. If no selected entries remain, selection clears and the list scrolls back inside the available results.

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
| Filter | Focus the name filter box (same as Ctrl+F) |

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Up / Down | Move selection (Shift extends range) |
| Enter | Open directory or file; or confirm pending delete |
| Backspace | Go up one directory |
| Delete | Request delete (second press confirms; multi-select OK) |
| Space | Properties for selection |
| Ctrl+A | Select all in folder |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut / paste selection (multi-select OK) |
| Ctrl+F | Filter the current folder listing by name |
| F2 | Start rename on selected entry (single item) |
| F5 | Refresh directory listing |
| Esc | Cancel rename, clear filter, cancel pending delete, or close context menu |

### Rename Mode

Press **F2** or choose Rename from the context menu. The caret starts at the end of the existing name. Use Left/Right/Home/End to move it, type to insert, Backspace/Delete to remove complete UTF-8 characters, then press **Enter** to commit or **Esc** to cancel. Names that contain `/` (or that are empty, `.`, or `..`) are rejected so rename cannot create a nested path.

If the renamed entry is open in Text Editor or Drawing, the shell updates that window's bound virtual path, title, session record, and singleton focus binding. Renaming a directory also updates open files and drawings below it, plus any Terminal or Filesystem Browser currently inside the directory.

### Filter / search

**Ctrl+F**, the toolbar **Filter** button, or the filter box on the right of the path bar focuses name search in the current folder.

- Type to filter the listing (case-insensitive substring).
- While filtering, Left/Right/Home/End move the caret, typed text is inserted at the caret, Delete removes the next complete UTF-8 character, and Backspace removes the previous one.
- Long filter queries stay at native text size and scroll horizontally to keep the caret visible.
- **Enter** keeps the filter and leaves typing mode.
- **Esc** clears the filter.
- Changing directory clears the filter.
- Changing directory also cancels an active rename, pending delete, or context menu so actions cannot target a row from the previous directory.

### Delete Confirmation

Deleting always requires confirmation (files and non-empty folders):

1. **Toolbar Delete** or **Delete** key: status bar asks to confirm; press **Delete** / **Enter** again to remove, or **Esc** to cancel. Any selection change, including Ctrl-click, Shift-range selection, or filtering away the target, cancels the pending delete.
2. **Context menu Delete**: submenu with **Confirm Delete** / **Cancel**.

Deletion uses `Filesystem::removeRecursive` (whole directory trees). The virtual root `/` cannot be deleted.

Deleting a file or folder that contains an open Editor or Drawing document does not close that app. The document keeps its in-memory content, releases the deleted path, and becomes an untitled tracked window so Save As or Save can choose a new destination. If a Terminal or Filesystem Browser is currently inside the deleted tree, it returns to the nearest existing parent directory. Deleted cut-clipboard entries are removed.

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

Right-clicking an already selected row keeps the current multi-selection, so context-menu Copy, Cut, Delete, and Properties can operate on the whole selection. Right-clicking an unselected row starts a new single selection.

## Copy, Cut, and Paste

- **Copy** or **Cut** the current selection (one item or multi-select) from the right-click menu (or Ctrl+C / Ctrl+X).
- **Paste** into the current directory from the right-click menu (or Ctrl+V).
- Cut + Paste moves items via `Filesystem::moveItemsInto`; Copy + Paste duplicates them, including directory trees via `copyItemsInto` → `copyRecursive`.
- Successful cut + paste moves notify open Text Editor and Drawing windows, so bound paths follow files and directories into their new location.
- A browser currently viewing a moved directory follows it and refreshes the listing at the new path.
- The shared virtual clipboard follows a successful rename or move made by another Filesystem Browser or Terminal, including sources nested under a moved directory.
- Paste skips items whose names already exist in the destination, same-folder sources, and folders pasted into themselves (`isSameOrDescendant`).
- Backspace in rename and filter prompts removes one UTF-8 codepoint at a time.
- Long names stay at their normal text size and are clipped within the list; while renaming, the visible text follows the caret so edits remain visible at either end of the name.

## Current Limitations

- Default open supports text + case-insensitive `.modr` only; force open-with can open any file in Editor or Drawing (Drawing rejects non-`.modr` loads).
- No drag-and-drop.
- The virtual clipboard is shared across Filesystem instances, but it is not connected to the host OS clipboard. Cut/paste uses non-overwriting moves; successful sources leave the cut clipboard, while a destination conflict leaves that source available for a later retry.

## Developer Notes

Main implementation files:

- `src/app/FilesystemApp.hpp`
- `src/app/FilesystemApp.cpp`
- `src/fs/Filesystem.*` — shared recursive copy/remove, path helpers, `fileSize`
- `src/window/detail/wm_body_07.inc` — `launchFilesystem()`
- `src/window/detail/wm_body_08.inc` / `wm_body_09.inc` — `openPath` / open-with bridges and shared virtual clipboard controller methods

File open uses `IWindowController` (`openPath`, `openInTextEditor`, `openInDrawing`) so the browser does not depend on Editor or Drawing classes.
