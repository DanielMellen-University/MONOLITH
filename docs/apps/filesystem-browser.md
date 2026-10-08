# Filesystem

A graphical file manager for the [internal filesystem](../filesystem.md). It is listed as **Filesystem** in the Start menu and on the desktop.

## Using it

The window has a path bar with a filter box on the right, a toolbar, the file list and a status bar. It starts in `/home/monolith`. You can open several windows (`Filesystem`, `Filesystem 2`, ...).

- Folders are shown with `▶` and files with `•`, folders first, sorted by name ignoring case.
- Double-click or press Enter on a folder to open it. On a file, this opens `.modr` files in Drawing and everything else in Text Editor.
- Up (toolbar) or Backspace goes to the parent folder.
- Space, or **Properties** in the context menu, shows name, path, type and size (or the number of items in a folder) in the status bar. With several items selected it shows totals.

### Toolbar

| Button | Action |
|--------|--------|
| Up | Go to the parent folder |
| New Folder | Create `New Folder`, then `New Folder 2` and so on |
| New File | Create `New File.txt`, then `New File 2.txt` and so on |
| Rename | Rename the selected item |
| Delete | Delete the selection (asks for confirmation) |
| Filter | Focus the filter box (same as Ctrl+F) |

### Selecting

- Click selects one item. Ctrl+click toggles an item. Shift+click or Shift+Up/Down selects a range.
- Ctrl+A selects everything in the folder.
- Copy, Cut, Delete and Properties work on the whole selection. Rename needs a single item.
- Right-clicking a selected item keeps the selection; right-clicking another item selects only that item.

### Context menus

| Right-click on | Menu |
|----------------|------|
| Empty space | New Folder, New File, Paste (when the clipboard has items), Refresh |
| A file | Open, Open with Text Editor, Open with Drawing, Properties, Copy, Cut, Paste, Rename, Delete |
| A folder | Open, Properties, Copy, Cut, Paste, Rename, Delete |

Paste appears in item menus only when the clipboard has items. Delete opens a submenu with **Confirm Delete** and **Cancel**.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| Up / Down | Move the selection (with Shift, extend it) |
| Enter | Open the selection, or confirm a pending delete |
| Backspace | Go to the parent folder |
| Delete | Ask to delete; press Delete or Enter again to confirm |
| Space | Properties |
| Ctrl+A | Select all |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut / paste items |
| Ctrl+F | Filter the folder by name |
| F2 | Rename |
| F5 | Refresh |
| Esc | Cancel rename, clear the filter, cancel a pending delete or close a menu |

While renaming or filtering, Left/Right/Home/End move the caret and Backspace/Delete remove characters.

## Rename

Press F2 or choose Rename. Edit the name in place, then Enter to save or Esc to cancel. Clicking elsewhere or scrolling cancels. Names that are empty, `.`, `..` or contain `/` are refused.

## Filter

Ctrl+F, the Filter button or a click on the filter box starts filtering. Typing shows only names that contain the text, ignoring case.

- Enter keeps the filter and stops typing. Esc clears it. Changing folder clears it.
- While typing in the filter, Ctrl+V pastes text into it. After Enter, Ctrl+V pastes files again.
- The filter is limited to 255 bytes of UTF-8, the Linux file name limit.
- F5 refreshes the listing while filtering.

## Delete

Deleting always asks first:

- **Toolbar or Delete key:** the status bar asks to confirm. Press Delete or Enter to delete, or Esc to cancel. Changing the selection also cancels.
- **Context menu:** choose **Confirm Delete** in the submenu.

Folders are deleted with everything in them. `/` cannot be deleted. If a deleted file is open in Text Editor or Drawing, that window keeps its content and becomes untitled. A Terminal or Filesystem window inside a deleted folder moves to the nearest parent.

## Copy, cut and paste

- Copy or Cut the selection, then Paste into the current folder.
- Paste never overwrites. Items whose names already exist, items pasted into their own folder and folders pasted into themselves are skipped. The status bar reports how many items were done and whether any were skipped.
- A cut item that could not be moved stays on the clipboard so you can try again.
- The clipboard is shared by all Filesystem windows and follows renames and moves made anywhere in MONOLITH. It is separate from the system clipboard.
- Moving a file or folder that is open in Text Editor or Drawing updates that window's path.

## Staying in sync

Changes made by other windows (Terminal, Text Editor, Drawing, another Filesystem window) refresh the folder you are viewing. A selected item stays selected when another window renames it. A refresh cancels an active rename so it cannot rename the wrong item.

## Limits

- Double-click opens `.modr` in Drawing and everything else in Text Editor. Use **Open with** to choose; Drawing only loads `.modr` files.
- No drag and drop.
- Long names and paths are clipped, not shrunk.

## Developer notes

- `src/app/FilesystemApp.{hpp,cpp}`: the browser.
- `src/window/detail/wm_body_07.inc`: `launchFilesystem()`.
- Opening files goes through `IWindowController::openPath`, `openInTextEditor` and `openInDrawing`, so the browser does not depend on the Editor or Drawing classes. The shared clipboard also lives behind `IWindowController`.
- Paste uses `Filesystem::copyItemsInto` and `moveItemsInto`; delete uses `removeRecursive`.
- Rendering and filtering details: [rendering.md](../internals/rendering.md#filesystem).
- Test: `test_filesystem_app_state`.
