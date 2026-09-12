#include "FilesystemApp.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace monolith::app {

namespace {
constexpr int kPathBarHeight = 28;
constexpr int kToolbarButtonHeight = 20;
constexpr int kToolbarPadding = 8;
constexpr int kToolbarGap = 6;
constexpr int kStatusBarHeight = 22;
constexpr int kStatusBarPadding = 8;

std::string parentVirtualPath(const std::string& normalizedPath) {
    if (normalizedPath.empty() || normalizedPath == "/") return "/";
    const size_t slash = normalizedPath.find_last_of('/');
    return slash == 0 ? "/" : normalizedPath.substr(0, slash);
}
}

FilesystemApp::FilesystemApp(TTF_Font* font, monolith::fs::Filesystem* fs)
    : m_font(font), m_fs(fs)
{
    if (m_fs) {
        // Make sure we start somewhere that exists
        if (!m_fs->isDirectory(m_currentPath)) {
            m_currentPath = "/";
            if (!m_fs->isDirectory(m_currentPath)) {
                m_currentPath = "/home";
                if (!m_fs->isDirectory(m_currentPath)) {
                    m_currentPath = "/";
                }
            }
        }
    }
    refreshEntries();
}

void FilesystemApp::onVirtualPathMoved(const std::string& oldPath,
                                       const std::string& newPath) {
    if (!m_fs) return;

    const std::string oldNormalized = m_fs->normalize(oldPath);
    const std::string newNormalized = m_fs->normalize(newPath);
    if (oldNormalized == newNormalized || oldNormalized == "/") return;
    const std::string current = m_fs->normalize(m_currentPath);
    if (m_fs->isSameOrDescendant(oldNormalized, current)) {
        m_currentPath = newNormalized + current.substr(oldNormalized.size());
        cancelPendingDelete();
        refreshEntries();
        setStatus("Folder moved: " + m_currentPath);
        return;
    }

    // A file or direct child folder can move through the directory currently
    // being viewed without moving the browser itself. Refresh that listing so
    // stale rows do not remain after an external rename or move.
    if (parentVirtualPath(oldNormalized) == current
        || parentVirtualPath(newNormalized) == current) {
        refreshEntries();
        setStatus("Listing updated");
    }
}

void FilesystemApp::onVirtualPathCreated(const std::string& path) {
    if (!m_fs) return;

    const std::string created = m_fs->normalize(path);
    const std::string current = m_fs->normalize(m_currentPath);
    // File-backed apps and recursive directory creation report the final path.
    // Refresh ancestor listings too so a newly created parent becomes visible
    // without requiring the user to press Refresh.
    if (parentVirtualPath(created) != current
        && !m_fs->isSameOrDescendant(current, created)) {
        return;
    }

    refreshEntries();
    setStatus("Listing updated");
}

void FilesystemApp::onVirtualPathChanged(const std::string& path) {
    if (!m_fs) return;

    const std::string changed = m_fs->normalize(path);
    const std::string current = m_fs->normalize(m_currentPath);
    if (parentVirtualPath(changed) != current) return;

    refreshEntries();
    setStatus("Listing updated");
}

void FilesystemApp::onVirtualPathRemoved(const std::string& path) {
    if (!m_fs) return;

    const std::string removed = m_fs->normalize(path);
    if (removed == "/") return;

    const std::string current = m_fs->normalize(m_currentPath);
    if (parentVirtualPath(removed) == current) {
        refreshEntries();
        setStatus("Listing updated");
        return;
    }
    if (!m_fs->isSameOrDescendant(removed, current)) return;

    std::string fallback = removed;
    const size_t slash = fallback.find_last_of('/');
    fallback = slash == 0 ? "/" : fallback.substr(0, slash);
    while (fallback != "/" && !m_fs->isDirectory(fallback)) {
        const size_t parentSlash = fallback.find_last_of('/');
        fallback = parentSlash == 0 ? "/" : fallback.substr(0, parentSlash);
    }
    setCurrentPath(fallback);
    setStatus("Folder removed; opened: " + m_currentPath);
}

void FilesystemApp::setCurrentPath(const std::string& virtualPath) {
    if (!m_fs) {
        setStatus("Filesystem not available");
        return;
    }

    std::string normalized = m_fs->normalize(virtualPath);
    if (m_fs->isDirectory(normalized)) {
        if (m_renaming) {
            finishRename(false);
        }
        closeContextMenu();
        cancelPendingDelete();
        m_currentPath = normalized;
        m_filtering = false;
        m_filterQuery.clear();
        m_filterCursorPos = 0;
        m_filterScrollPx = 0;
        refreshEntries();
        clearMultiSelection();
        m_selectedIndex = -1;
        m_scrollOffset = 0;
        setStatus("Opened: " + m_currentPath);
    } else {
        setStatus("Open failed: not a directory");
    }
}

void FilesystemApp::goUp() {
    if (!m_fs) {
        setStatus("Filesystem not available");
        return;
    }

    if (m_currentPath == "/" || m_currentPath.empty()) return;

    // Compute parent via .. + normalize (reuses FS canonicalization for robustness;
    // matches the join+normalize style used in Terminal for consistent path rules).
    std::string candidate = m_currentPath + "/..";
    std::string parent = m_fs->normalize(candidate);
    if (parent.empty()) parent = "/";

    setCurrentPath(parent);
}

void FilesystemApp::refreshEntries() {
    // A refresh can follow an external rename, move, or delete. Do not leave
    // a menu target pointing at an index from the previous listing.
    if (m_showContextMenu) {
        closeContextMenu();
    }

    using SelectionIdentity = std::pair<std::string, bool>;
    std::set<SelectionIdentity> selectedIdentities;
    for (const int index : selectedIndicesSorted()) {
        if (index >= 0 && index < static_cast<int>(m_entries.size())) {
            const auto& entry = m_entries[static_cast<size_t>(index)];
            selectedIdentities.emplace(entry.name, entry.isDirectory);
        }
    }

    bool hadPrimarySelection = m_selectedIndex >= 0
        && m_selectedIndex < static_cast<int>(m_entries.size());
    SelectionIdentity primaryIdentity;
    if (hadPrimarySelection) {
        const auto& entry = m_entries[static_cast<size_t>(m_selectedIndex)];
        primaryIdentity = {entry.name, entry.isDirectory};
    }

    const bool hadAnchor = m_anchorIndex >= 0
        && m_anchorIndex < static_cast<int>(m_entries.size());
    SelectionIdentity anchorIdentity;
    if (hadAnchor) {
        const auto& entry = m_entries[static_cast<size_t>(m_anchorIndex)];
        anchorIdentity = {entry.name, entry.isDirectory};
    }

    m_entries.clear();
    if (!m_fs) {
        clearMultiSelection();
        m_selectedIndex = -1;
        return;
    }

    auto raw = m_fs->listEntries(m_currentPath);
    m_entries = monolith::fs::Filesystem::filterEntries(raw, m_filterQuery);

    clearMultiSelection();
    int restoredPrimary = -1;
    int restoredAnchor = -1;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto& entry = m_entries[i];
        const SelectionIdentity identity{entry.name, entry.isDirectory};
        if (hadAnchor && identity == anchorIdentity) {
            restoredAnchor = static_cast<int>(i);
        }
        if (!selectedIdentities.count(identity)) continue;

        const int index = static_cast<int>(i);
        m_selectedSet.insert(index);
        if (hadPrimarySelection && identity == primaryIdentity) {
            restoredPrimary = index;
        }
    }

    if (restoredPrimary >= 0) {
        m_selectedIndex = restoredPrimary;
    } else if (!m_selectedSet.empty()) {
        // If the old primary disappeared, keep the remaining selection usable
        // by promoting the first surviving entry.
        m_selectedIndex = *m_selectedSet.begin();
    } else {
        m_selectedIndex = -1;
    }

    m_anchorIndex = restoredAnchor >= 0
        ? restoredAnchor
        : m_selectedIndex;

    clampSelection();

    std::set<SelectionIdentity> restoredIdentities;
    for (const int index : selectedIndicesSorted()) {
        if (index >= 0 && index < static_cast<int>(m_entries.size())) {
            const auto& entry = m_entries[static_cast<size_t>(index)];
            restoredIdentities.emplace(entry.name, entry.isDirectory);
        }
    }
    bool restoredPrimarySelection = m_selectedIndex >= 0
        && m_selectedIndex < static_cast<int>(m_entries.size());
    SelectionIdentity restoredPrimaryIdentity;
    if (restoredPrimarySelection) {
        const auto& entry = m_entries[static_cast<size_t>(m_selectedIndex)];
        restoredPrimaryIdentity = {entry.name, entry.isDirectory};
    }
    if (m_confirmingDelete
        && (selectedIdentities != restoredIdentities
            || hadPrimarySelection != restoredPrimarySelection
            || (hadPrimarySelection && restoredPrimarySelection
                && primaryIdentity != restoredPrimaryIdentity))) {
        cancelPendingDelete();
    }
}

std::string FilesystemApp::fullPathFor(const std::string& name) const {
    // Use normalize after construction for consistent rules with Terminal (no raw manual glue).
    if (m_currentPath == "/" || m_currentPath.empty()) {
        return m_fs ? m_fs->normalize("/" + name) : ("/" + name);
    }
    return m_fs ? m_fs->normalize(m_currentPath + "/" + name) : (m_currentPath + "/" + name);
}

void FilesystemApp::openFileEntry(const std::string& name, const char* forceApp) {
    if (!m_fs) {
        setStatus("Open failed: filesystem not available");
        return;
    }
    const std::string path = fullPathFor(name);
    if (!m_fs->isFile(path)) {
        setStatus("Open failed: not a regular file");
        return;
    }

    if (!getController()) return;

    auto* ctrl = getController();
    if (forceApp && std::string(forceApp) == "drawing") {
        ctrl->openInDrawing(path);
        return;
    }
    if (forceApp && std::string(forceApp) == "editor") {
        ctrl->openInTextEditor(path);
        return;
    }
    // Default shell routing (.modr → Drawing, else Editor).
    ctrl->openPath(path);
}

void FilesystemApp::activateEntry(size_t index) {
    if (index >= m_entries.size()) return;

    const auto& entry = m_entries[index];

    if (entry.isDirectory) {
        std::string newPath = fullPathFor(entry.name);
        setCurrentPath(newPath);
    } else {
        openFileEntry(entry.name);
    }
}

void FilesystemApp::createNewFolder() {
    if (!m_fs) {
        setStatus("Create folder failed: filesystem not available");
        return;
    }

    std::string base = "New Folder";
    std::string candidate = base;
    int counter = 2;

    // Find a free name
    while (m_fs->exists(fullPathFor(candidate))) {
        std::ostringstream oss;
        oss << base << " " << counter++;
        candidate = oss.str();
    }

    const std::string path = fullPathFor(candidate);
    if (m_fs->createDirectory(path)) {
        if (auto* ctrl = getController()) {
            ctrl->notifyVirtualPathCreated(path);
        } else {
            refreshEntries();
        }
        selectEntryNamed(candidate, true);
        setStatus("Created folder: " + candidate);
    } else {
        setStatus("Create folder failed: " + candidate);
    }
}

void FilesystemApp::createNewFile() {
    if (!m_fs) {
        setStatus("Create file failed: filesystem not available");
        return;
    }

    std::string base = "New File.txt";
    std::string candidate = base;
    int counter = 2;

    while (m_fs->exists(fullPathFor(candidate))) {
        std::ostringstream oss;
        oss << "New File " << counter++ << ".txt";
        candidate = oss.str();
    }

    const std::string path = fullPathFor(candidate);
    if (m_fs->writeFile(path, "")) {
        if (auto* ctrl = getController()) {
            ctrl->notifyVirtualPathCreated(path);
        } else {
            refreshEntries();
        }
        if (selectEntryNamed(candidate, false)) {
            startRenameSelected();
            setStatus("Created file: " + candidate);
        }
    } else {
        setStatus("Create file failed: " + candidate);
    }
}

std::string FilesystemApp::entryBaseName(const std::string& virtualPath) const {
    if (!m_fs) return virtualPath;
    const std::string normalized = m_fs->normalize(virtualPath);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) return normalized;
    return normalized.substr(slash + 1);
}

void FilesystemApp::clearMultiSelection() {
    m_selectedSet.clear();
    m_anchorIndex = -1;
}

bool FilesystemApp::isIndexSelected(int index) const {
    if (m_selectedSet.count(index)) return true;
    return index == m_selectedIndex;
}

std::vector<int> FilesystemApp::selectedIndicesSorted() const {
    std::set<int> combined = m_selectedSet;
    if (m_selectedIndex >= 0) combined.insert(m_selectedIndex);
    return std::vector<int>(combined.begin(), combined.end());
}

int FilesystemApp::primarySelectedIndex() const {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_entries.size())) {
        return m_selectedIndex;
    }
    auto sorted = selectedIndicesSorted();
    return sorted.empty() ? -1 : sorted.front();
}

void FilesystemApp::toggleSelection(int index) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;
    if (m_confirmingDelete) {
        cancelPendingDelete();
    }
    if (m_selectedSet.count(index)) {
        m_selectedSet.erase(index);
        if (m_selectedIndex == index) {
            m_selectedIndex = m_selectedSet.empty() ? -1 : *m_selectedSet.begin();
        }
        if (m_anchorIndex == index) {
            m_anchorIndex = m_selectedIndex;
        }
    } else {
        m_selectedSet.insert(index);
        m_selectedIndex = index;
        m_anchorIndex = index;
    }
}

void FilesystemApp::selectRange(int fromIndex, int toIndex) {
    if (m_entries.empty()) return;
    if (m_confirmingDelete) {
        cancelPendingDelete();
    }
    fromIndex = std::clamp(fromIndex, 0, static_cast<int>(m_entries.size()) - 1);
    toIndex = std::clamp(toIndex, 0, static_cast<int>(m_entries.size()) - 1);
    if (fromIndex > toIndex) std::swap(fromIndex, toIndex);
    m_selectedSet.clear();
    for (int i = fromIndex; i <= toIndex; ++i) {
        m_selectedSet.insert(i);
    }
    m_selectedIndex = toIndex;
}

void FilesystemApp::showPropertiesForSelection() {
    auto indices = selectedIndicesSorted();
    if (indices.empty() || !m_fs) {
        setStatus("Properties: nothing selected");
        return;
    }
    if (indices.size() > 1) {
        int files = 0, dirs = 0;
        for (int i : indices) {
            if (i < 0 || i >= static_cast<int>(m_entries.size())) continue;
            if (m_entries[static_cast<size_t>(i)].isDirectory) ++dirs;
            else ++files;
        }
        std::ostringstream oss;
        oss << "Properties: " << indices.size() << " items (" << dirs << " folders, "
            << files << " files)";
        setStatus(oss.str());
        return;
    }

    const int idx = indices.front();
    const auto& e = m_entries[static_cast<size_t>(idx)];
    const std::string path = fullPathFor(e.name);
    std::ostringstream oss;
    oss << e.name << "  |  " << path << "  |  ";
    if (e.isDirectory) {
        oss << "folder";
        auto kids = m_fs->listEntries(path);
        oss << "  |  " << kids.size() << " entries";
    } else {
        oss << "file";
        std::uint64_t bytes = 0;
        if (m_fs->fileSize(path, bytes)) {
            if (bytes < 1024) {
                oss << "  |  " << bytes << " B";
            } else if (bytes < 1024ull * 1024ull) {
                oss << "  |  " << (bytes / 1024) << " KB";
            } else {
                oss << "  |  " << (bytes / (1024ull * 1024ull)) << " MB";
            }
        }
    }
    setStatus(oss.str());
}

bool FilesystemApp::readClipboard(std::vector<std::string>& paths, bool& isCut) const {
    if (auto* ctrl = getController()) {
        return ctrl->getFilesystemClipboard(paths, isCut);
    }
    paths = m_clipboardPaths;
    isCut = m_clipboardIsCut;
    return !paths.empty();
}

bool FilesystemApp::hasClipboard() const {
    std::vector<std::string> paths;
    bool isCut = false;
    return readClipboard(paths, isCut) && !paths.empty();
}

void FilesystemApp::writeClipboard(const std::vector<std::string>& paths, bool isCut) {
    m_clipboardPaths = paths;
    m_clipboardIsCut = isCut;
    if (auto* ctrl = getController()) {
        ctrl->setFilesystemClipboard(paths, isCut);
    }
}

void FilesystemApp::clearClipboard() {
    m_clipboardPaths.clear();
    m_clipboardIsCut = false;
    if (auto* ctrl = getController()) {
        ctrl->clearFilesystemClipboard();
    }
}

void FilesystemApp::copySelectedToClipboard(bool cut) {
    auto indices = selectedIndicesSorted();
    if (!m_fs || indices.empty()) {
        setStatus(cut ? "Cut failed: no item selected" : "Copy failed: no item selected");
        return;
    }

    std::vector<std::string> paths;
    paths.reserve(indices.size());
    for (int idx : indices) {
        if (idx < 0 || idx >= static_cast<int>(m_entries.size())) continue;
        paths.push_back(fullPathFor(m_entries[static_cast<size_t>(idx)].name));
    }
    if (paths.empty()) {
        setStatus(cut ? "Cut failed: no item selected" : "Copy failed: no item selected");
        return;
    }
    writeClipboard(paths, cut);
    if (paths.size() == 1) {
        setStatus((cut ? "Cut: " : "Copied: ") + entryBaseName(paths.front()));
    } else {
        setStatus((cut ? "Cut: " : "Copied: ") + std::to_string(paths.size()) + " items");
    }
}

void FilesystemApp::pasteFromClipboard() {
    if (!m_fs) {
        setStatus("Paste failed: filesystem not available");
        return;
    }
    std::vector<std::string> clipboardPaths;
    bool clipboardIsCut = false;
    if (!readClipboard(clipboardPaths, clipboardIsCut) || clipboardPaths.empty()) {
        setStatus("Paste failed: clipboard is empty");
        return;
    }

    std::vector<std::string> sources;
    sources.reserve(clipboardPaths.size());
    for (const auto& src : clipboardPaths) {
        if (m_fs->exists(src)) {
            sources.push_back(src);
        }
    }
    if (sources.empty()) {
        setStatus("Paste failed: source no longer exists");
        clearClipboard();
        return;
    }

    const bool wasCut = clipboardIsCut;
    std::vector<std::string> candidateNames;
    std::vector<std::pair<std::string, std::string>> candidateMoves;
    candidateNames.reserve(sources.size());
    candidateMoves.reserve(sources.size());
    for (const auto& src : sources) {
        const std::string name = m_fs->baseName(src);
        const std::string dest = fullPathFor(name);
        if (src == dest || m_fs->isSameOrDescendant(src, dest) || m_fs->exists(dest)) continue;
        candidateNames.push_back(name);
        candidateMoves.emplace_back(src, dest);
    }
    const int copied = wasCut
        ? m_fs->moveItemsInto(sources, m_currentPath)
        : m_fs->copyItemsInto(sources, m_currentPath);

    if (wasCut) {
        // Keep only sources that still exist. A partial move should leave
        // destination-conflicted items available for retry, not already-moved
        // paths that can never be pasted again.
        std::vector<std::string> remainingPaths;
        remainingPaths.reserve(clipboardPaths.size());
        for (const auto& source : clipboardPaths) {
            if (m_fs->exists(source)) {
                remainingPaths.push_back(source);
            }
        }
        if (remainingPaths.empty()) {
            clearClipboard();
        } else if (remainingPaths.size() != clipboardPaths.size()) {
            writeClipboard(remainingPaths, true);
        }

        if (auto* ctrl = getController()) {
            for (const auto& [source, destination] : candidateMoves) {
                if (!m_fs->exists(source) && m_fs->exists(destination)) {
                    ctrl->notifyVirtualPathMoved(source, destination);
                }
            }
        }
    } else if (auto* ctrl = getController()) {
        for (const auto& [source, destination] : candidateMoves) {
            if (m_fs->exists(source) && m_fs->exists(destination)) {
                ctrl->notifyVirtualPathCreated(destination);
            }
        }
    }

    refreshEntries();
    if (copied == 0) {
        setStatus("Paste failed: nothing copied (name exists, same folder, or blocked)");
        return;
    }
    if (copied == 1) {
        std::string name;
        for (const auto& candidate : candidateNames) {
            if (m_fs->exists(fullPathFor(candidate))) {
                name = candidate;
                break;
            }
        }
        if (!name.empty()) {
            selectEntryNamed(name, m_fs->isDirectory(fullPathFor(name)));
            setStatus((wasCut ? "Moved: " : "Pasted: ") + name);
        } else {
            setStatus(wasCut ? "Moved 1 item" : "Pasted 1 item");
        }
    } else {
        setStatus(wasCut
            ? ("Moved: " + std::to_string(copied) + " items")
            : ("Pasted: " + std::to_string(copied) + " items"));
    }
}

void FilesystemApp::cancelPendingDelete() {
    m_confirmingDelete = false;
    m_pendingDeleteIndex = -1;
}

void FilesystemApp::requestDeleteSelected() {
    auto indices = selectedIndicesSorted();
    if (!m_fs || indices.empty()) {
        setStatus(m_fs ? "Delete failed: no item selected" : "Delete failed: filesystem not available");
        cancelPendingDelete();
        return;
    }

    // Second Delete/Enter while confirming → perform.
    if (m_confirmingDelete) {
        performDeleteSelected();
        return;
    }

    m_confirmingDelete = true;
    m_pendingDeleteIndex = primarySelectedIndex();
    if (indices.size() == 1) {
        const auto& e = m_entries[static_cast<size_t>(indices.front())];
        setStatus(std::string("Delete \"") + e.name + (e.isDirectory ? "\" (folder tree)" : "\"")
                  + "? Delete/Enter confirm, Esc cancel");
    } else {
        setStatus("Delete " + std::to_string(indices.size())
                  + " items? Delete/Enter confirm, Esc cancel");
    }
}

void FilesystemApp::performDeleteSelected() {
    auto indices = selectedIndicesSorted();
    if (!m_fs || indices.empty()) {
        setStatus(m_fs ? "Delete failed: no item selected" : "Delete failed: filesystem not available");
        cancelPendingDelete();
        return;
    }

    int okCount = 0;
    int failCount = 0;
    // Delete from the end so earlier indices stay valid while iterating.
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        const int idx = *it;
        if (idx < 0 || idx >= static_cast<int>(m_entries.size())) continue;
        const std::string name = m_entries[static_cast<size_t>(idx)].name;
        const std::string target = fullPathFor(name);
        if (m_fs->normalize(target) == "/") {
            ++failCount;
            continue;
        }
        if (m_fs->removeRecursive(target)) {
            ++okCount;
            if (auto* ctrl = getController()) {
                ctrl->notifyVirtualPathRemoved(target);
            }
        } else {
            ++failCount;
        }
    }

    cancelPendingDelete();
    refreshEntries();
    if (!m_entries.empty()) {
        setSelection(std::min(indices.front(), static_cast<int>(m_entries.size()) - 1));
    } else {
        m_selectedIndex = -1;
    }
    ensureSelectionVisible();

    if (failCount == 0) {
        setStatus("Deleted " + std::to_string(okCount) + " item(s)");
    } else {
        setStatus("Deleted " + std::to_string(okCount) + ", failed " + std::to_string(failCount));
    }
}

void FilesystemApp::startRenameSelected() {
    const int idx = primarySelectedIndex();
    if (!m_fs || idx < 0 || idx >= static_cast<int>(m_entries.size())) {
        setStatus(m_fs ? "Rename failed: no item selected" : "Rename failed: filesystem not available");
        return;
    }
    if (selectedIndicesSorted().size() > 1) {
        setStatus("Rename: select a single item");
        return;
    }

    m_selectedIndex = idx;
    m_renaming = true;
    m_renameIndex = idx;
    m_renameBuffer = m_entries[static_cast<size_t>(idx)].name;
    m_renameCursorPos = m_renameBuffer.size();
    setStatus("Renaming: " + m_renameBuffer);
}

void FilesystemApp::finishRename(bool commit) {
    if (!m_renaming || m_renameIndex < 0 || m_renameIndex >= static_cast<int>(m_entries.size())) {
        m_renaming = false;
        m_renameIndex = -1;
        m_renameBuffer.clear();
        m_renameCursorPos = 0;
        return;
    }

    const auto& entry = m_entries[m_renameIndex];
    std::string oldName = entry.name;
    bool wasDirectory = entry.isDirectory;

    if (commit && m_renameBuffer.empty()) {
        setStatus("Rename failed: name cannot be empty");
    } else if (commit && !monolith::fs::Filesystem::isValidEntryName(m_renameBuffer)) {
        setStatus("Rename failed: name cannot contain /");
    } else if (commit && m_renameBuffer != entry.name) {
        if (m_fs->exists(fullPathFor(m_renameBuffer))) {
            setStatus("Rename failed: name already exists");
        } else if (m_fs->renameEntry(m_currentPath, oldName, m_renameBuffer)) {
            if (auto* ctrl = getController()) {
                ctrl->notifyVirtualPathMoved(
                    fullPathFor(oldName), fullPathFor(m_renameBuffer));
            }
            refreshEntries();
            selectEntryNamed(m_renameBuffer, wasDirectory);
            setStatus("Renamed " + oldName + " to " + m_renameBuffer);
        } else {
            setStatus("Rename failed: " + oldName);
        }
    } else if (!commit) {
        setStatus("Rename canceled");
    } else {
        setStatus("Rename unchanged");
    }

    m_renaming = false;
    m_renameIndex = -1;
    m_renameBuffer.clear();
    m_renameCursorPos = 0;
}

void FilesystemApp::beginFilter() {
    m_filtering = true;
    if (m_renaming) {
        finishRename(false);
    }
    closeContextMenu();
    m_filterCursorPos = m_filterQuery.size();
    m_filterScrollPx = 0;
    setStatus(m_filterQuery.empty()
        ? "Filter: type to search this folder (Enter keep, Esc clear)"
        : ("Filter: " + m_filterQuery));
}

void FilesystemApp::clearFilter() {
    const bool hadFilter = m_filtering || !m_filterQuery.empty();
    m_filtering = false;
    m_filterQuery.clear();
    m_filterCursorPos = 0;
    m_filterScrollPx = 0;
    if (hadFilter) {
        refreshEntries();
        setStatus("Filter cleared");
    }
}

void FilesystemApp::applyFilterQuery() {
    refreshEntries();
    if (m_filterQuery.empty()) {
        setStatus("Filter: (all items)");
    } else {
        setStatus("Filter: " + m_filterQuery + "  (" + std::to_string(m_entries.size()) + " match"
                  + (m_entries.size() == 1 ? ")" : "es)"));
    }
}

void FilesystemApp::setSelection(int index, bool additive) {
    if (m_entries.empty()) {
        m_selectedIndex = -1;
        clearMultiSelection();
        if (m_confirmingDelete) cancelPendingDelete();
        return;
    }
    if (index < 0) index = 0;
    if (index >= static_cast<int>(m_entries.size())) index = static_cast<int>(m_entries.size()) - 1;
    if (m_confirmingDelete) {
        cancelPendingDelete();
    }
    if (!additive) {
        m_selectedSet.clear();
        m_selectedSet.insert(index);
        m_anchorIndex = index;
    } else {
        m_selectedSet.insert(index);
        m_anchorIndex = index;
    }
    m_selectedIndex = index;
}

bool FilesystemApp::selectEntryNamed(const std::string& name, bool isDirectory) {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].name == name && m_entries[i].isDirectory == isDirectory) {
            setSelection(static_cast<int>(i), false);
            ensureSelectionVisible();
            return true;
        }
    }
    return false;
}

void FilesystemApp::clampSelection() {
    if (m_entries.empty()) {
        m_selectedIndex = -1;
        clearMultiSelection();
        m_scrollOffset = 0;
        return;
    }
    // Drop out-of-range multi-select indices.
    for (auto it = m_selectedSet.begin(); it != m_selectedSet.end(); ) {
        if (*it < 0 || *it >= static_cast<int>(m_entries.size())) it = m_selectedSet.erase(it);
        else ++it;
    }
    if (m_selectedIndex >= static_cast<int>(m_entries.size())) {
        m_selectedIndex = static_cast<int>(m_entries.size()) - 1;
    }
    if (m_selectedIndex < -1) {
        m_selectedIndex = -1;
    }
    if (m_selectedIndex >= 0) {
        m_selectedSet.insert(m_selectedIndex);
    }

    const int visible = std::max(1, getVisibleRowCount({0, 0, m_clientWidth, m_clientHeight}));
    const int maxScroll = std::max(0, static_cast<int>(m_entries.size()) - visible);
    m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);
    ensureSelectionVisible();
}

void FilesystemApp::ensureSelectionVisible() {
    if (m_selectedIndex < 0) return;

    int visible = getVisibleRowCount({0, 0, m_clientWidth, m_clientHeight});
    if (visible <= 0) visible = 8;

    if (m_selectedIndex < m_scrollOffset) {
        m_scrollOffset = m_selectedIndex;
    } else if (m_selectedIndex >= m_scrollOffset + visible) {
        m_scrollOffset = m_selectedIndex - visible + 1;
    }
    if (m_scrollOffset < 0) m_scrollOffset = 0;
}

int FilesystemApp::getVisibleRowCount(const SDL_Rect& contentRect) const {
    int rh = getRowHeight();
    if (rh <= 0) return 10;

    const int reservedTop = getListTop();
    const int reservedBottom = getStatusBarHeight() + kStatusBarPadding;
    int available = contentRect.h - reservedTop - reservedBottom;
    if (available <= 0) return 0;
    return std::max(1, available / rh);
}

void FilesystemApp::handleMouseButton(const SDL_MouseButtonEvent& e) {
    const int mx = e.x;
    const int my = e.y;

    // === Context menu handling (takes priority) ===
    if (m_showContextMenu) {
        if (e.button == SDL_BUTTON_LEFT) {
            int clickedItem = contextMenuItemAt(mx, my);
            if (clickedItem != -1) {
                executeContextMenuAction(clickedItem);
            } else {
                closeContextMenu();
            }
        } else if (e.button == SDL_BUTTON_RIGHT) {
            closeContextMenu();
        }
        return;
    }

    // === Right click → show context menu ===
    if (e.button == SDL_BUTTON_RIGHT) {
        const int listTop = getListTop();
        if (my >= listTop && my < m_clientHeight - getStatusBarHeight()) {
            int rowHeight = getRowHeight();
            if (rowHeight <= 0) rowHeight = 20;

            int relY = my - listTop;
            int clickedRow = m_scrollOffset + (relY / rowHeight);

            if (clickedRow >= 0 && clickedRow < static_cast<int>(m_entries.size())) {
                // Preserve a multi-selection when the context menu is opened on
                // one of its rows. Right-clicking an unselected row starts a new
                // single selection, matching ordinary click behavior.
                if (isIndexSelected(clickedRow)) {
                    m_selectedIndex = clickedRow;
                    if (m_confirmingDelete) cancelPendingDelete();
                } else {
                    setSelection(clickedRow);
                }
                showContextMenu(mx, my, clickedRow);
            } else {
                clearMultiSelection();
                m_selectedIndex = -1;
                showContextMenu(mx, my, -1);
            }
        } else {
            clearMultiSelection();
            m_selectedIndex = -1;
            showContextMenu(mx, my, -1);
        }
        return;
    }

    // === Left click handling ===
    if (e.button != SDL_BUTTON_LEFT) return;

    // Check toolbar buttons first
    SDL_Point pt { mx, my };
    if (SDL_PointInRect(&pt, &m_btnUp)) {
        goUp();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnNewFolder)) {
        createNewFolder();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnNewFile)) {
        createNewFile();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnDelete)) {
        requestDeleteSelected();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnRename)) {
        startRenameSelected();
        return;
    }
    if (SDL_PointInRect(&pt, &m_btnFilter)) {
        beginFilter();
        return;
    }
    if (SDL_PointInRect(&pt, &m_filterHitRect)) {
        beginFilter();
        return;
    }

    // If we click anywhere while renaming, finish (cancel) the rename
    if (m_renaming) {
        finishRename(false);
    }

    // Path bar + toolbar area is above the list
    const int listTop = getListTop();
    if (my < listTop || my >= m_clientHeight - getStatusBarHeight()) return;

    // Compute which row was clicked
    int rowHeight = getRowHeight();
    if (rowHeight <= 0) rowHeight = 20;

    int relY = my - listTop;
    if (relY < 0) return;

    int clickedRow = m_scrollOffset + (relY / rowHeight);
    if (clickedRow >= 0 && clickedRow < static_cast<int>(m_entries.size())) {
        const bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (shift && m_anchorIndex >= 0) {
            selectRange(m_anchorIndex, clickedRow);
        } else if (ctrl) {
            toggleSelection(clickedRow);
        } else {
            setSelection(clickedRow, false);
        }

        if (e.clicks >= 2 && !ctrl && !shift) {
            activateEntry(static_cast<size_t>(clickedRow));
        }
    } else if ((SDL_GetModState() & (KMOD_CTRL | KMOD_SHIFT)) == 0) {
        clearMultiSelection();
        m_selectedIndex = -1;
    }
}

void FilesystemApp::handleMouseWheel(const SDL_MouseWheelEvent& e) {
    int visible = getVisibleRowCount({0, 0, m_clientWidth, m_clientHeight});
    int maxScroll = std::max(0, static_cast<int>(m_entries.size()) - visible);

    m_scrollOffset -= e.y * 3; // a few rows per wheel tick
    if (m_scrollOffset < 0) m_scrollOffset = 0;
    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
}

void FilesystemApp::handleKeyDown(const SDL_Keysym& keysym) {
    if (m_renaming) {
        // Handle rename mode specially
        if (keysym.sym == SDLK_RETURN || keysym.sym == SDLK_KP_ENTER) {
            finishRename(true);
            return;
        }
        if (keysym.sym == SDLK_ESCAPE) {
            finishRename(false);
            return;
        }
        if (keysym.sym == SDLK_BACKSPACE) {
            erasePreviousUtf8Codepoint(m_renameBuffer, m_renameCursorPos);
            return;
        }
        if (keysym.sym == SDLK_DELETE) {
            const std::size_t next = utf8NextCodepointStart(m_renameBuffer, m_renameCursorPos);
            if (next > m_renameCursorPos) {
                m_renameBuffer.erase(m_renameCursorPos, next - m_renameCursorPos);
            }
            return;
        }
        if (keysym.sym == SDLK_LEFT) {
            m_renameCursorPos = utf8PrevCodepointStart(m_renameBuffer, m_renameCursorPos);
            return;
        }
        if (keysym.sym == SDLK_RIGHT) {
            m_renameCursorPos = utf8NextCodepointStart(m_renameBuffer, m_renameCursorPos);
            return;
        }
        if (keysym.sym == SDLK_HOME) {
            m_renameCursorPos = 0;
            return;
        }
        if (keysym.sym == SDLK_END) {
            m_renameCursorPos = m_renameBuffer.size();
            return;
        }
        return; // Ignore other keys while renaming
    }

    if (m_filtering) {
        if (keysym.sym == SDLK_RETURN || keysym.sym == SDLK_KP_ENTER) {
            m_filtering = false;
            applyFilterQuery();
            return;
        }
        if (keysym.sym == SDLK_ESCAPE) {
            clearFilter();
            return;
        }
        if (keysym.sym == SDLK_BACKSPACE) {
            erasePreviousUtf8Codepoint(m_filterQuery, m_filterCursorPos);
            applyFilterQuery();
            return;
        }
        if (keysym.sym == SDLK_DELETE) {
            m_filterCursorPos = std::min(m_filterCursorPos, m_filterQuery.size());
            const std::size_t next = utf8NextCodepointStart(m_filterQuery, m_filterCursorPos);
            if (next > m_filterCursorPos) {
                m_filterQuery.erase(m_filterCursorPos, next - m_filterCursorPos);
                applyFilterQuery();
            }
            return;
        }
        if (keysym.sym == SDLK_LEFT) {
            m_filterCursorPos = utf8PrevCodepointStart(m_filterQuery, m_filterCursorPos);
            return;
        }
        if (keysym.sym == SDLK_RIGHT) {
            m_filterCursorPos = utf8NextCodepointStart(m_filterQuery, m_filterCursorPos);
            return;
        }
        if (keysym.sym == SDLK_HOME) {
            m_filterCursorPos = 0;
            return;
        }
        if (keysym.sym == SDLK_END) {
            m_filterCursorPos = m_filterQuery.size();
            return;
        }
        if (keysym.sym == SDLK_UP || keysym.sym == SDLK_DOWN) {
            // Allow moving the selection while the filter is applied.
        } else {
            return;
        }
    }

    if (m_confirmingDelete) {
        if (keysym.sym == SDLK_ESCAPE) {
            cancelPendingDelete();
            setStatus("Delete canceled");
            return;
        }
        if (keysym.sym == SDLK_RETURN || keysym.sym == SDLK_KP_ENTER || keysym.sym == SDLK_DELETE) {
            performDeleteSelected();
            return;
        }
    }

    switch (keysym.sym) {
        case SDLK_UP:
            if (m_selectedIndex > 0) {
                if (keysym.mod & KMOD_SHIFT) {
                    if (m_anchorIndex < 0) m_anchorIndex = m_selectedIndex;
                    selectRange(m_anchorIndex, m_selectedIndex - 1);
                } else {
                    setSelection(m_selectedIndex - 1, false);
                }
                ensureSelectionVisible();
            }
            break;

        case SDLK_DOWN:
            if (m_selectedIndex + 1 < static_cast<int>(m_entries.size())) {
                if (keysym.mod & KMOD_SHIFT) {
                    if (m_anchorIndex < 0) m_anchorIndex = m_selectedIndex;
                    selectRange(m_anchorIndex, m_selectedIndex + 1);
                } else {
                    setSelection(m_selectedIndex + 1, false);
                }
                ensureSelectionVisible();
            } else if (m_selectedIndex < 0 && !m_entries.empty()) {
                setSelection(0, false);
                ensureSelectionVisible();
            }
            break;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (m_selectedIndex >= 0) {
                activateEntry(static_cast<size_t>(m_selectedIndex));
            }
            break;

        case SDLK_BACKSPACE:
            goUp();
            break;

        case SDLK_DELETE:
            requestDeleteSelected();
            break;

        case SDLK_F2:
            startRenameSelected();
            break;

        case SDLK_F5:
            refreshEntries();
            setStatus("Refreshed");
            break;

        case SDLK_f:
            if (keysym.mod & KMOD_CTRL) {
                beginFilter();
                return;
            }
            break;

        case SDLK_a:
            if (keysym.mod & KMOD_CTRL) {
                if (!m_entries.empty()) {
                    selectRange(0, static_cast<int>(m_entries.size()) - 1);
                    setStatus("Selected " + std::to_string(m_entries.size()) + " items");
                }
            }
            break;

        case SDLK_SPACE:
            // Space shows properties (without activating).
            if (!(keysym.mod & KMOD_CTRL)) {
                showPropertiesForSelection();
            }
            break;

        case SDLK_c:
            if (keysym.mod & KMOD_CTRL) {
                copySelectedToClipboard(false);
            }
            break;

        case SDLK_x:
            if (keysym.mod & KMOD_CTRL) {
                copySelectedToClipboard(true);
            }
            break;

        case SDLK_v:
            if (keysym.mod & KMOD_CTRL) {
                pasteFromClipboard();
            }
            break;

        case SDLK_ESCAPE:
            if (m_showContextMenu) {
                closeContextMenu();
            } else if (m_renaming) {
                finishRename(false);
            } else if (m_filtering || !m_filterQuery.empty()) {
                clearFilter();
            }
            break;

        default:
            break;
    }
}

void FilesystemApp::handleEvent(const SDL_Event& event) {
    if (m_renaming && event.type == SDL_TEXTINPUT) {
        if (event.text.text) {
            m_renameCursorPos = std::min(m_renameCursorPos, m_renameBuffer.size());
            const std::string inserted = event.text.text;
            m_renameBuffer.insert(m_renameCursorPos, inserted);
            m_renameCursorPos += inserted.size();
        }
        return;
    }

    if (m_filtering && event.type == SDL_TEXTINPUT) {
        if (event.text.text) {
            m_filterCursorPos = std::min(m_filterCursorPos, m_filterQuery.size());
            const std::string inserted = event.text.text;
            m_filterQuery.insert(m_filterCursorPos, inserted);
            m_filterCursorPos += inserted.size();
            applyFilterQuery();
        }
        return;
    }

    if (event.type == SDL_MOUSEMOTION && m_showContextMenu) {
        m_contextMenuHoverIndex = contextMenuItemAt(event.motion.x, event.motion.y);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        handleMouseButton(event.button);
    } else if (event.type == SDL_MOUSEWHEEL) {
        handleMouseWheel(event.wheel);
    } else if (event.type == SDL_KEYDOWN) {
        handleKeyDown(event.key.keysym);
    }
}

int FilesystemApp::getRowHeight() const {
    if (!m_font) return 20;
    return TTF_FontHeight(m_font) + 4; // small padding between rows
}

int FilesystemApp::getPathBarHeight() const {
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 16;
    return std::max(kPathBarHeight, fontHeight + 6);
}

int FilesystemApp::getToolbarY() const {
    return getPathBarHeight() + 2;
}

int FilesystemApp::getToolbarButtonHeight() const {
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 16;
    return std::max(kToolbarButtonHeight, fontHeight + 2);
}

int FilesystemApp::getListTop() const {
    return getToolbarY() + getToolbarButtonHeight() + 2;
}

int FilesystemApp::getStatusBarHeight() const {
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 16;
    return std::max(kStatusBarHeight, fontHeight + 4);
}

void FilesystemApp::drawPathBar(SDL_Renderer* r, const SDL_Rect& contentRect, int& outTopY) {
    const int pathBarHeight = getPathBarHeight();
    SDL_Rect bar = {
        contentRect.x,
        contentRect.y,
        contentRect.w,
        pathBarHeight
    };

    // Slightly different background for the path area
    SDL_SetRenderDrawColor(r, 28, 30, 36, 255);
    SDL_RenderFillRect(r, &bar);

    // Current path text
    const int filterBoxW = std::min(180, std::max(90, contentRect.w / 3));
    m_filterHitRect = {contentRect.w - filterBoxW - 8, 4, filterBoxW, pathBarHeight - 8};
    if (m_font) {
        SDL_Color pathColor = {180, 190, 200, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, m_currentPath.c_str(), pathColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                const int pathVisibleWidth = std::max(1, contentRect.w - filterBoxW - 24);
                SDL_Rect pathClip = {
                    contentRect.x + 10,
                    contentRect.y,
                    pathVisibleWidth,
                    pathBarHeight
                };
                SDL_RenderSetClipRect(r, &pathClip);
                SDL_Rect dst = {
                    contentRect.x + 10,
                    contentRect.y + (pathBarHeight - surf->h) / 2,
                    surf->w,
                    surf->h
                };
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_RenderSetClipRect(r, nullptr);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        SDL_Rect filterDraw = {
            contentRect.x + m_filterHitRect.x,
            contentRect.y + m_filterHitRect.y,
            m_filterHitRect.w,
            m_filterHitRect.h
        };
        if (m_filtering) {
            SDL_SetRenderDrawColor(r, 55, 70, 95, 255);
        } else {
            SDL_SetRenderDrawColor(r, 36, 38, 44, 255);
        }
        SDL_RenderFillRect(r, &filterDraw);
        SDL_SetRenderDrawColor(r, 70, 75, 85, 255);
        SDL_RenderDrawRect(r, &filterDraw);

        std::string filterLabel;
        int filterCursorPx = 0;
        if (m_filtering) {
            m_filterCursorPos = std::min(m_filterCursorPos, m_filterQuery.size());
            const std::string beforeCursor = m_filterQuery.substr(0, m_filterCursorPos);
            filterLabel = beforeCursor + "_" + m_filterQuery.substr(m_filterCursorPos);
            int cursorHeight = 0;
            TTF_SizeUTF8(m_font, (beforeCursor + "_").c_str(), &filterCursorPx, &cursorHeight);
        } else {
            filterLabel = m_filterQuery.empty() ? "Filter..." : m_filterQuery;
            m_filterScrollPx = 0;
        }
        SDL_Color filterCol = m_filterQuery.empty() && !m_filtering
            ? SDL_Color{120, 125, 130, 255}
            : SDL_Color{210, 215, 220, 255};
        SDL_Surface* fs = TTF_RenderUTF8_Blended(m_font, filterLabel.c_str(), filterCol);
        if (fs) {
            SDL_Texture* ft = SDL_CreateTextureFromSurface(r, fs);
            if (ft) {
                const int visibleWidth = std::max(1, filterDraw.w - 12);
                if (m_filtering) {
                    if (filterCursorPx - m_filterScrollPx > visibleWidth) {
                        m_filterScrollPx = filterCursorPx - visibleWidth;
                    } else if (filterCursorPx - m_filterScrollPx < 0) {
                        m_filterScrollPx = filterCursorPx;
                    }
                    const int maxScroll = std::max(0, fs->w - visibleWidth);
                    m_filterScrollPx = std::clamp(m_filterScrollPx, 0, maxScroll);
                }
                SDL_Rect filterClip = {
                    filterDraw.x + 6,
                    filterDraw.y,
                    visibleWidth,
                    filterDraw.h
                };
                SDL_RenderSetClipRect(r, &filterClip);
                SDL_Rect dst = {
                    filterDraw.x + 6 - (m_filtering ? m_filterScrollPx : 0),
                    filterDraw.y + (filterDraw.h - fs->h) / 2,
                    fs->w,
                    fs->h
                };
                SDL_RenderCopy(r, ft, nullptr, &dst);
                SDL_RenderSetClipRect(r, nullptr);
                SDL_DestroyTexture(ft);
            }
            SDL_FreeSurface(fs);
        }
    }

    outTopY = contentRect.y + pathBarHeight;
}

void FilesystemApp::drawToolbar(SDL_Renderer* r, const SDL_Rect& contentRect) {
    const int toolbarY = getToolbarY();
    const int toolbarButtonHeight = getToolbarButtonHeight();
    // Store button rects in relative coordinates (for hit testing with relative mouse events)
    int relX = kToolbarPadding;

    auto drawButton = [&](SDL_Rect& outRect, const char* label, int w) {
        // Store relative rect for input
        outRect = {relX, toolbarY, w, toolbarButtonHeight};

        // Draw using absolute screen coordinates
        SDL_Rect drawRect = {
            contentRect.x + relX,
            contentRect.y + toolbarY,
            w,
            toolbarButtonHeight
        };

        // Button background
        SDL_SetRenderDrawColor(r, 45, 48, 55, 255);
        SDL_RenderFillRect(r, &drawRect);

        // Subtle border
        SDL_SetRenderDrawColor(r, 70, 75, 85, 255);
        SDL_RenderDrawRect(r, &drawRect);

        if (m_font) {
            SDL_Color col = {210, 215, 220, 255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, label, col);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) {
                    SDL_Rect dst = {
                        drawRect.x + (drawRect.w - surf->w) / 2,
                        drawRect.y + (drawRect.h - surf->h) / 2,
                        surf->w, surf->h
                    };
                    SDL_RenderCopy(r, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        relX += w + kToolbarGap;
    };

    drawButton(m_btnUp, "Up", 48);
    drawButton(m_btnNewFolder, "New Folder", 92);
    drawButton(m_btnNewFile, "New File", 80);
    drawButton(m_btnRename, "Rename", 68);
    drawButton(m_btnDelete, "Delete", 68);
    drawButton(m_btnFilter, "Filter", 56);
}

void FilesystemApp::drawList(SDL_Renderer* r, const SDL_Rect& contentRect, int listTopY) {
    if (!m_font) {
        SDL_SetRenderDrawColor(r, 20, 20, 24, 255);
        SDL_RenderFillRect(r, &contentRect);
        return;
    }

    const int rowH = getRowHeight();
    (void)rowH; // used via getVisibleRowCount

    const int listHeight = std::max(
        0, contentRect.h - (listTopY - contentRect.y) - 6 - getStatusBarHeight());

    SDL_Rect listArea = {
        contentRect.x,
        listTopY,
        contentRect.w,
        listHeight
    };

    // List background
    SDL_SetRenderDrawColor(r, 22, 22, 26, 255);
    SDL_RenderFillRect(r, &listArea);

    if (m_entries.empty()) {
        SDL_Color dim = {140, 145, 150, 255};
        const char* emptyMsg = m_filterQuery.empty() ? "(empty directory)" : "(no matching items)";
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, emptyMsg, dim);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_Rect dst = {listArea.x + 16, listArea.y + 10, surf->w, surf->h};
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
        return;
    }

    SDL_Color textNormal   = {205, 210, 215, 255};
    SDL_Color textDir      = {170, 200, 240, 255};  // slightly blue for directories
    SDL_Color selBg        = {55, 70, 95, 255};
    SDL_Color selText      = {230, 235, 245, 255};

    int y = listTopY + 2;
    const int visible = listHeight > 0
        ? std::min(getVisibleRowCount(contentRect), listHeight / rowH)
        : 0;

    for (int i = 0; i < visible; ++i) {
        int entryIdx = m_scrollOffset + i;
        if (entryIdx >= static_cast<int>(m_entries.size())) break;

        const auto& entry = m_entries[entryIdx];
        bool isSelected = isIndexSelected(entryIdx);
        bool isPrimary = (entryIdx == m_selectedIndex);
        bool isRenamingThis = m_renaming && (entryIdx == m_renameIndex);

        SDL_Rect rowRect = {
            listArea.x,
            y,
            listArea.w,
            rowH - 1
        };

        if (isSelected && !isRenamingThis) {
            if (isPrimary) {
                SDL_SetRenderDrawColor(r, selBg.r, selBg.g, selBg.b, 255);
            } else {
                SDL_SetRenderDrawColor(r, 42, 52, 70, 255);
            }
            SDL_RenderFillRect(r, &rowRect);
        }

        if (isRenamingThis) {
            // Highlight rename row more strongly
            SDL_SetRenderDrawColor(r, 70, 90, 120, 255);
            SDL_RenderFillRect(r, &rowRect);
        }

        // Small indicator for directory vs file
        const char* indicator = entry.isDirectory ? "▶" : "•";
        SDL_Color indColor = entry.isDirectory ? textDir : textNormal;

        // Indicator
        {
            SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, indicator, indColor);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
                if (t) {
                    SDL_Rect d = {rowRect.x + 8, rowRect.y + 2, s->w, s->h};
                    SDL_RenderCopy(r, t, nullptr, &d);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Name (or rename buffer if renaming this row)
        {
            std::string displayText = isRenamingThis ? m_renameBuffer : entry.name;
            SDL_Color nameCol = isRenamingThis ? selText : (isSelected ? selText : (entry.isDirectory ? textDir : textNormal));

            const int nameX = rowRect.x + 28;
            const int nameWidth = std::max(1, rowRect.w - 36);
            int textW = 0;
            int textH = 0;
            TTF_SizeUTF8(m_font, displayText.c_str(), &textW, &textH);
            const std::size_t cursorPos = std::min(m_renameCursorPos, displayText.size());
            const std::string beforeCursor = displayText.substr(0, cursorPos);
            int prefixW = 0;
            int prefixH = 0;
            if (isRenamingThis) {
                TTF_SizeUTF8(m_font, beforeCursor.c_str(), &prefixW, &prefixH);
            }
            const int maxTextOffset = std::max(0, textW - nameWidth);
            const int cursorMargin = std::max(0, nameWidth - 2);
            const int textOffset = isRenamingThis
                ? std::clamp(prefixW - cursorMargin, 0, maxTextOffset)
                : 0;

            SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, displayText.c_str(), nameCol);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
                if (t) {
                    SDL_Rect nameClip = {nameX, rowRect.y, nameWidth, rowRect.h};
                    SDL_RenderSetClipRect(r, &nameClip);
                    SDL_Rect d = {nameX - textOffset, rowRect.y + 2, s->w, s->h};
                    SDL_RenderCopy(r, t, nullptr, &d);
                    SDL_RenderSetClipRect(r, nullptr);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }

            // Draw a simple cursor when renaming
            if (isRenamingThis) {
                const int cursorX = nameX + prefixW - textOffset + 1;
                int cursorY = rowRect.y + 2;
                SDL_SetRenderDrawColor(r, 255, 255, 255, 220);
                SDL_Rect nameClip = {nameX, rowRect.y, nameWidth, rowH};
                SDL_RenderSetClipRect(r, &nameClip);
                SDL_RenderDrawLine(r, cursorX, cursorY, cursorX, cursorY + rowH - 6);
                SDL_RenderSetClipRect(r, nullptr);
            }
        }

        y += rowH;
    }
}

void FilesystemApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    m_clientWidth = contentRect.w;
    m_clientHeight = contentRect.h;

    // Main background
    SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    SDL_RenderFillRect(renderer, &contentRect);

    if (!m_font) {
        return;
    }

    int listTop = 0;
    drawPathBar(renderer, contentRect, listTop);
    drawToolbar(renderer, contentRect);
    drawList(renderer, contentRect, contentRect.y + getListTop());
    drawStatusBar(renderer, contentRect);

    if (m_showContextMenu) {
        drawContextMenu(renderer, contentRect);
    }
}

void FilesystemApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    clampSelection();
    if (m_showContextMenu) {
        updateContextMenuLayout();
    }
}

void FilesystemApp::onUiScaleChanged() {
    // The filter prompt stores its horizontal position in pixels; remeasure it
    // against the new font on the next render.
    m_filterScrollPx = 0;
    clampSelection();
    if (m_showContextMenu) {
        updateContextMenuLayout();
    }
}

void FilesystemApp::showContextMenu(int x, int y, int targetIndex) {
    closeContextMenu(); // close any existing menu first

    m_contextMenuPos = {x, y};
    m_contextMenuTarget = targetIndex;
    m_contextMenuHoverIndex = -1;
    m_contextMenuItems.clear();

    if (targetIndex == -1) {
        // Background menu
        m_contextMenuItems.push_back("New Folder");
        m_contextMenuItems.push_back("New File");
        if (hasClipboard()) {
            m_contextMenuItems.push_back("Paste");
        }
        m_contextMenuItems.push_back("Refresh");
    } else if (targetIndex >= 0 && targetIndex < static_cast<int>(m_entries.size())) {
        const auto& entry = m_entries[targetIndex];
        m_contextMenuItems.push_back("Open");
        if (!entry.isDirectory) {
            m_contextMenuItems.push_back("Open with Text Editor");
            m_contextMenuItems.push_back("Open with Drawing");
        }
        m_contextMenuItems.push_back("Properties");
        m_contextMenuItems.push_back("Copy");
        m_contextMenuItems.push_back("Cut");
        if (hasClipboard()) {
            m_contextMenuItems.push_back("Paste");
        }
        m_contextMenuItems.push_back("Rename");
        m_contextMenuItems.push_back("Delete");
    }

    updateContextMenuLayout();
    m_showContextMenu = !m_contextMenuItems.empty();
}

void FilesystemApp::closeContextMenu() {
    m_showContextMenu = false;
    m_confirmingDelete = false;
    m_contextMenuItems.clear();
    m_contextMenuHoverIndex = -1;
    m_contextMenuTarget = -1;
    m_contextMenuRect = {0, 0, 0, 0};
}

void FilesystemApp::executeContextMenuAction(int menuIndex) {
    if (!m_showContextMenu || menuIndex < 0 || menuIndex >= static_cast<int>(m_contextMenuItems.size())) {
        closeContextMenu();
        return;
    }

    std::string action = m_contextMenuItems[menuIndex];
    int target = m_contextMenuTarget;   // Save before closing!
    SDL_Point menuPos = m_contextMenuPos;
    closeContextMenu();

    if (action == "New Folder") {
        createNewFolder();
    } else if (action == "New File") {
        createNewFile();
    } else if (action == "Refresh") {
        refreshEntries();
    } else if (action == "Open") {
        if (target >= 0) {
            setSelection(target, false);
            activateEntry(static_cast<size_t>(target));
        }
    } else if (action == "Open with Text Editor") {
        if (target >= 0 && target < static_cast<int>(m_entries.size())
            && !m_entries[static_cast<size_t>(target)].isDirectory) {
            openFileEntry(m_entries[static_cast<size_t>(target)].name, "editor");
        }
    } else if (action == "Open with Drawing") {
        if (target >= 0 && target < static_cast<int>(m_entries.size())
            && !m_entries[static_cast<size_t>(target)].isDirectory) {
            openFileEntry(m_entries[static_cast<size_t>(target)].name, "drawing");
        }
    } else if (action == "Properties") {
        if (target >= 0) {
            if (!isIndexSelected(target)) {
                setSelection(target, false);
            }
            showPropertiesForSelection();
        }
    } else if (action == "Copy") {
        if (target >= 0) {
            if (!isIndexSelected(target)) {
                setSelection(target, false);
            }
            copySelectedToClipboard(false);
        }
    } else if (action == "Cut") {
        if (target >= 0) {
            if (!isIndexSelected(target)) {
                setSelection(target, false);
            }
            copySelectedToClipboard(true);
        }
    } else if (action == "Paste") {
        pasteFromClipboard();
    } else if (action == "Rename") {
        if (target >= 0) {
            setSelection(target, false);
            startRenameSelected();
        }
    } else if (action == "Delete") {
        if (target >= 0) {
            if (!isIndexSelected(target)) {
                setSelection(target, false);
            }
            m_confirmingDelete = true;
            m_pendingDeleteIndex = target;
            m_contextMenuPos = menuPos;
            m_contextMenuTarget = target;
            m_contextMenuItems.clear();
            m_contextMenuItems.push_back("Confirm Delete");
            m_contextMenuItems.push_back("Cancel");
            updateContextMenuLayout();
            m_showContextMenu = true;
            auto indices = selectedIndicesSorted();
            if (indices.size() == 1) {
                const std::string& name = m_entries[static_cast<size_t>(target)].name;
                setStatus("Delete \"" + name + "\"? Confirm in menu or Enter");
            } else {
                setStatus("Delete " + std::to_string(indices.size())
                          + " items? Confirm in menu or Enter");
            }
        }
    } else if (action == "Confirm Delete") {
        if (target >= 0 && !isIndexSelected(target)) {
            setSelection(target, false);
        }
        performDeleteSelected();
    } else if (action == "Cancel") {
        cancelPendingDelete();
        setStatus("Delete canceled");
    }
}

void FilesystemApp::updateContextMenuLayout() {
    const int itemHeight = getRowHeight() + 2;
    const int padding = 6;
    const int minWidth = 140;

    if (m_contextMenuItems.empty() || !m_font) {
        m_contextMenuRect = {0, 0, 0, 0};
        return;
    }

    int maxTextWidth = minWidth;
    for (const auto& item : m_contextMenuItems) {
        int w = 0, h = 0;
        TTF_SizeUTF8(m_font, item.c_str(), &w, &h);
        if (w > maxTextWidth) maxTextWidth = w;
    }

    int menuWidth = maxTextWidth + padding * 2 + 20;
    int menuHeight = static_cast<int>(m_contextMenuItems.size()) * itemHeight + padding * 2;
    int menuX = m_contextMenuPos.x;
    int menuY = m_contextMenuPos.y;

    if (menuX + menuWidth > m_clientWidth) {
        menuX = m_clientWidth - menuWidth - 4;
    }
    if (menuY + menuHeight > m_clientHeight) {
        menuY = m_clientHeight - menuHeight - 4;
    }
    if (menuX < 4) menuX = 4;
    if (menuY < 4) menuY = 4;

    m_contextMenuRect = {menuX, menuY, menuWidth, menuHeight};
}

int FilesystemApp::contextMenuItemAt(int x, int y) const {
    if (!m_showContextMenu || m_contextMenuItems.empty()) {
        return -1;
    }

    SDL_Point point{x, y};
    if (!SDL_PointInRect(&point, &m_contextMenuRect)) {
        return -1;
    }

    const int itemHeight = getRowHeight() + 2;
    const int padding = 6;
    int itemY = m_contextMenuRect.y + padding;

    for (size_t i = 0; i < m_contextMenuItems.size(); ++i) {
        SDL_Rect itemRect = {
            m_contextMenuRect.x,
            itemY,
            m_contextMenuRect.w,
            itemHeight
        };
        if (SDL_PointInRect(&point, &itemRect)) {
            return static_cast<int>(i);
        }
        itemY += itemHeight;
    }

    return -1;
}

void FilesystemApp::drawStatusBar(SDL_Renderer* r, const SDL_Rect& contentRect) {
    const int statusBarHeight = getStatusBarHeight();
    SDL_Rect bar = {
        contentRect.x,
        contentRect.y + contentRect.h - statusBarHeight,
        contentRect.w,
        statusBarHeight
    };

    SDL_SetRenderDrawColor(r, 30, 32, 38, 255);
    SDL_RenderFillRect(r, &bar);

    if (!m_font) return;

    std::string status;

    if (m_entries.empty()) {
        status = "0 items";
    } else {
        status = std::to_string(m_entries.size()) + " items";
    }

    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_entries.size())) {
        const auto& sel = m_entries[m_selectedIndex];
        status += "   |   Selected: " + sel.name;
        if (sel.isDirectory) status += " (dir)";
    }

    if (!m_statusMessage.empty()) {
        status += "   |   " + m_statusMessage;
    }

    SDL_Color textCol = {160, 165, 175, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, status.c_str(), textCol);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        if (tex) {
            const int visibleWidth = std::max(1, contentRect.w - 20);
            SDL_Rect statusClip = {
                contentRect.x + 10,
                bar.y,
                visibleWidth,
                bar.h
            };
            SDL_RenderSetClipRect(r, &statusClip);
            SDL_Rect dst = {
                contentRect.x + 10,
                bar.y + (statusBarHeight - surf->h) / 2,
                surf->w,
                surf->h
            };
            SDL_RenderCopy(r, tex, nullptr, &dst);
            SDL_RenderSetClipRect(r, nullptr);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}

void FilesystemApp::setStatus(const std::string& message) {
    m_statusMessage = message;
}

void FilesystemApp::drawContextMenu(SDL_Renderer* r, const SDL_Rect& contentRect) {
    if (!m_showContextMenu || m_contextMenuItems.empty() || !m_font) return;

    const int itemHeight = getRowHeight() + 2;
    const int padding = 6;

    // Convert stored relative menu rect to absolute screen coordinates
    int menuX = contentRect.x + m_contextMenuRect.x;
    int menuY = contentRect.y + m_contextMenuRect.y;

    SDL_Rect menuRect = {menuX, menuY, m_contextMenuRect.w, m_contextMenuRect.h};

    // Background
    SDL_SetRenderDrawColor(r, 38, 40, 48, 255);
    SDL_RenderFillRect(r, &menuRect);

    // Border
    SDL_SetRenderDrawColor(r, 70, 75, 85, 255);
    SDL_RenderDrawRect(r, &menuRect);

    // Menu items
    int itemY = menuY + padding;
    for (size_t i = 0; i < m_contextMenuItems.size(); ++i) {
        bool hovered = (static_cast<int>(i) == m_contextMenuHoverIndex);

        SDL_Rect itemRect = {menuX + 1, itemY, m_contextMenuRect.w - 2, itemHeight - 1};

        if (hovered) {
            SDL_SetRenderDrawColor(r, 60, 80, 110, 255);
            SDL_RenderFillRect(r, &itemRect);
        }

        SDL_Color textCol = hovered ? SDL_Color{230, 235, 245, 255} : SDL_Color{200, 205, 215, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, m_contextMenuItems[i].c_str(), textCol);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex) {
                SDL_Rect dst = {
                    menuX + padding + 4,
                    itemY + (itemHeight - surf->h) / 2,
                    surf->w, surf->h
                };
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        itemY += itemHeight;
    }
}

} // namespace monolith::app
