// Headless test of shipped Filesystem multi-item copy/paste, rename, and listing filter.
// Compiles against src/fs/Filesystem.cpp (no SDL).

#include "../src/detail/AtomicFile.hpp"
#include "../src/fs/Filesystem.hpp"
#include "TestTempDir.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>

namespace stdfs = std::filesystem;
using monolith::fs::Filesystem;

class ScopedDescriptorPressure {
public:
    ~ScopedDescriptorPressure() { restore(); }

    bool apply() {
        if (::getrlimit(RLIMIT_NOFILE, &originalLimit_) != 0
            || originalLimit_.rlim_cur <= 16) {
            return false;
        }

        struct rlimit restrictedLimit = originalLimit_;
        restrictedLimit.rlim_cur = std::min<rlim_t>(32, originalLimit_.rlim_cur);
        descriptors_.reserve(static_cast<std::size_t>(restrictedLimit.rlim_cur));
        if (::setrlimit(RLIMIT_NOFILE, &restrictedLimit) != 0) return false;
        active_ = true;

        while (true) {
            const int descriptor = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            if (descriptor < 0) {
                reachedLimit_ = errno == EMFILE;
                break;
            }
            descriptors_.push_back(descriptor);
        }
        return true;
    }

    bool reachedLimit() const { return reachedLimit_; }

    bool restore() {
        for (const int descriptor : descriptors_) ::close(descriptor);
        descriptors_.clear();
        if (!active_) return false;
        if (::setrlimit(RLIMIT_NOFILE, &originalLimit_) != 0) return false;
        active_ = false;
        return true;
    }

private:
    struct rlimit originalLimit_ {};
    std::vector<int> descriptors_;
    bool active_ = false;
    bool reachedLimit_ = false;
};

static std::string referenceNormalize(const std::string& path) {
    if (path.empty()) return "/";

    std::vector<std::string> parts;
    std::istringstream input(path);
    std::string part;
    while (std::getline(input, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(part);
        }
    }

    std::string result = "/";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "/";
        result += parts[i];
    }
    return result;
}

static std::string referenceJoin(const std::string& directory,
                                 const std::string& child) {
    if (child.empty()) {
        return referenceNormalize(directory.empty() ? "/" : directory);
    }

    std::string joined = directory.empty() ? "/" : directory;
    if (joined == "/") return referenceNormalize("/" + child);
    if (joined.back() != '/') joined.push_back('/');
    joined += child;
    return referenceNormalize(joined);
}

static bool hasAtomicTempWorkspace(const stdfs::path& directory) {
    for (const auto& entry : stdfs::directory_iterator(directory)) {
        if (entry.path().filename().string().starts_with(".monolith-tmp-")) {
            return true;
        }
    }
    return false;
}

static bool writeFixture(const stdfs::path& path, std::string_view contents) {
    std::ofstream file(path, std::ios::binary);
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(file);
}

static bool referenceIsSameOrDescendant(const std::string& ancestor,
                                        const std::string& path) {
    const std::string normalizedAncestor = referenceNormalize(ancestor);
    const std::string normalizedPath = referenceNormalize(path);
    if (normalizedAncestor.empty() || normalizedPath.empty()) return false;
    if (normalizedAncestor == normalizedPath) return true;
    if (normalizedAncestor == "/") return true;

    std::string prefix = normalizedAncestor;
    if (prefix.back() != '/') prefix.push_back('/');
    return normalizedPath.compare(0, prefix.size(), prefix) == 0;
}

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << "FAIL: " << msg << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << msg << '\n';
        }
    };

    monolith::test::ScopedTempDirectory hostTemp("monolith-fs-roadmap");
    if (!hostTemp) {
        std::cerr << "FAIL: could not create temp host root\n";
        return 1;
    }
    const stdfs::path hostRoot = hostTemp.path();
    std::error_code ec;

    const stdfs::path maintenanceDeep = hostRoot / "maintenance-unvisited" / "deep";
    std::string maintenanceWorkspaceName;
    const bool maintenanceWorkspaceNameReady =
        monolith::detail::createAtomicTempTokenName(maintenanceWorkspaceName);
    const stdfs::path maintenanceStaleWorkspace = maintenanceDeep / maintenanceWorkspaceName;
    ec.clear();
    const bool maintenanceFixturesReady = maintenanceWorkspaceNameReady
        && stdfs::create_directories(maintenanceDeep, ec) && !ec
        && stdfs::create_directory(maintenanceStaleWorkspace, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(maintenanceStaleWorkspace);

    const stdfs::path maintenanceLookalike = maintenanceDeep
        / ".monolith-tmp-v4-lookalike";
    ec.clear();
    const bool maintenanceLookalikeReady =
        stdfs::create_directory(maintenanceLookalike, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(maintenanceLookalike)
        && writeFixture(maintenanceLookalike / "user-data", "keep");

    const stdfs::path maintenanceRestricted = hostRoot / "maintenance-restricted";
    const stdfs::path restrictedStaleWorkspace = maintenanceRestricted
        / ".monolith-tmp-v3-unreadable";
    ec.clear();
    const bool restrictedWorkspaceReady = stdfs::create_directory(maintenanceRestricted, ec)
        && !ec && stdfs::create_directory(restrictedStaleWorkspace, ec) && !ec
        && writeFixture(restrictedStaleWorkspace / "lease", "")
        && writeFixture(restrictedStaleWorkspace / "ready", "")
        && writeFixture(restrictedStaleWorkspace / "content", "restricted");
    ec.clear();
    const auto restrictedPermissions = stdfs::status(maintenanceRestricted, ec).permissions();
    const bool restrictedPermissionsRead = restrictedWorkspaceReady && !ec;
    ec.clear();
    stdfs::permissions(maintenanceRestricted, stdfs::perms::none,
                       stdfs::perm_options::replace, ec);
    const bool restrictedPermissionsApplied = restrictedPermissionsRead && !ec;
    ec.clear();
    stdfs::directory_iterator restrictedProbe(maintenanceRestricted, ec);
    const bool restrictedActuallyDenied = ec == std::errc::permission_denied;

    monolith::test::ScopedTempDirectory maintenanceOutsideTemp(
        "monolith-fs-maintenance-outside");
    const stdfs::path outsideWorkspace = maintenanceOutsideTemp.path()
        / ".monolith-tmp-v3-outside";
    ec.clear();
    const bool outsideFixturesReady = maintenanceOutsideTemp
        && stdfs::create_directory(outsideWorkspace, ec) && !ec
        && writeFixture(outsideWorkspace / "lease", "")
        && writeFixture(outsideWorkspace / "ready", "")
        && writeFixture(outsideWorkspace / "content", "outside");
    ec.clear();
    if (outsideFixturesReady) {
        stdfs::create_directory_symlink(maintenanceOutsideTemp.path(),
                                         hostRoot / "outside-link", ec);
    }
    const bool outsideLinkReady = outsideFixturesReady && !ec;

    Filesystem fs(hostRoot.string());
    check(fs.initialize(), "filesystem initialize");
    const bool maintenancePendingAfterFirstEntry = fs.maintenanceStep(1);
    const bool firstMaintenanceStepWasBounded = maintenancePendingAfterFirstEntry
        && stdfs::exists(maintenanceStaleWorkspace);
    std::size_t maintenanceSteps = 1;
    bool maintenancePending = maintenancePendingAfterFirstEntry;
    while (maintenancePending && maintenanceSteps < 1000) {
        maintenancePending = fs.maintenanceStep(1);
        ++maintenanceSteps;
    }
    ec.clear();
    stdfs::permissions(maintenanceRestricted, restrictedPermissions,
                       stdfs::perm_options::replace, ec);
    const bool restrictedPermissionsRestored = !ec;
    check(maintenanceFixturesReady && firstMaintenanceStepWasBounded
              && !maintenancePending && maintenanceSteps < 1000
              && !stdfs::exists(maintenanceStaleWorkspace),
          "bounded maintenance reclaims an abandoned workspace in an untouched nested directory");
    check(restrictedWorkspaceReady && restrictedPermissionsApplied
              && (restrictedActuallyDenied || ::geteuid() == 0)
              && restrictedPermissionsRestored
              && (!restrictedActuallyDenied || stdfs::exists(restrictedStaleWorkspace)),
          "maintenance continues through accessible siblings when a directory cannot be read");
    check(maintenanceLookalikeReady && stdfs::exists(maintenanceLookalike / "user-data")
              && outsideLinkReady && stdfs::exists(outsideWorkspace),
          "maintenance preserves unknown workspace data and does not follow directory symlinks");

    const stdfs::path lockContentionRoot = hostRoot / "maintenance-lock-contention";
    std::string lockContentionWorkspaceName;
    ec.clear();
    const bool lockContentionFixturesReady =
        monolith::detail::createAtomicTempTokenName(lockContentionWorkspaceName)
        && stdfs::create_directory(lockContentionRoot, ec) && !ec;
    const stdfs::path lockContentionWorkspace = lockContentionRoot
        / lockContentionWorkspaceName;
    ec.clear();
    const bool lockContentionWorkspaceReady = lockContentionFixturesReady
        && stdfs::create_directory(lockContentionWorkspace, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(lockContentionWorkspace);
    Filesystem lockContentionFs(lockContentionRoot.string());
    const bool lockContentionFsReady = lockContentionWorkspaceReady
        && lockContentionFs.initialize();
    monolith::detail::AtomicTempParentLock heldParentLock(lockContentionRoot);
    std::promise<void> cleanupStarted;
    auto cleanupStartedFuture = cleanupStarted.get_future();
    auto cleanupFuture = std::async(std::launch::async, [&]() {
        cleanupStarted.set_value();
        return lockContentionFs.maintenanceStep(32);
    });
    cleanupStartedFuture.wait();
    const bool returnedWithoutParentLock = cleanupFuture.wait_for(
        std::chrono::milliseconds(500)) == std::future_status::ready;
    heldParentLock.release();
    const bool remainedPendingWhileContended = cleanupFuture.get();
    const bool workspacePreservedWhileContended =
        stdfs::exists(lockContentionWorkspace);
    bool lockContentionPending = remainedPendingWhileContended;
    std::size_t lockContentionSteps = 0;
    while (lockContentionPending && lockContentionSteps < 1000) {
        lockContentionPending = lockContentionFs.maintenanceStep(32);
        ++lockContentionSteps;
    }
    check(lockContentionFsReady && returnedWithoutParentLock
              && remainedPendingWhileContended && workspacePreservedWhileContended
              && !lockContentionPending && lockContentionSteps < 1000
              && !stdfs::exists(lockContentionWorkspace),
          "startup cleanup avoids blocking on a busy parent and retries after release");

    const stdfs::path descriptorRoot = hostRoot / "maintenance-root-descriptor-limit";
    std::string rootDescriptorWorkspaceName;
    ec.clear();
    const bool rootDescriptorFixturesReady =
        monolith::detail::createAtomicTempTokenName(rootDescriptorWorkspaceName)
        && stdfs::create_directory(descriptorRoot, ec) && !ec;
    const stdfs::path rootDescriptorWorkspace = descriptorRoot
        / rootDescriptorWorkspaceName;
    ec.clear();
    const bool rootDescriptorWorkspaceReady = rootDescriptorFixturesReady
        && stdfs::create_directory(rootDescriptorWorkspace, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(rootDescriptorWorkspace);
    Filesystem rootDescriptorFs(descriptorRoot.string());
    ScopedDescriptorPressure rootDescriptorPressure;
    const bool rootDescriptorLimitApplied = rootDescriptorWorkspaceReady
        && rootDescriptorPressure.apply();
    const bool rootDescriptorLimitReached = rootDescriptorPressure.reachedLimit();
    const bool rootDescriptorFsReady = rootDescriptorLimitApplied
        && rootDescriptorFs.initialize();
    const bool rootDescriptorLimitRestored = rootDescriptorPressure.restore();
    std::size_t rootDescriptorMaintenanceSteps = 0;
    bool rootDescriptorMaintenancePending = rootDescriptorFsReady
        && rootDescriptorFs.maintenanceStep(32);
    while (rootDescriptorMaintenancePending && rootDescriptorMaintenanceSteps < 1000) {
        rootDescriptorMaintenancePending = rootDescriptorFs.maintenanceStep(32);
        ++rootDescriptorMaintenanceSteps;
    }
    check(rootDescriptorWorkspaceReady && rootDescriptorLimitApplied
              && rootDescriptorLimitReached && rootDescriptorFsReady
              && rootDescriptorLimitRestored && !rootDescriptorMaintenancePending
              && rootDescriptorMaintenanceSteps < 1000
              && !stdfs::exists(rootDescriptorWorkspace),
          "startup cleanup retries its root after initialization hits descriptor exhaustion");

    const stdfs::path candidateLockDescriptorRoot =
        hostRoot / "maintenance-candidate-lock-descriptor-limit";
    std::string candidateLockWorkspaceName;
    ec.clear();
    const bool candidateLockFixturesReady =
        monolith::detail::createAtomicTempTokenName(candidateLockWorkspaceName)
        && stdfs::create_directory(candidateLockDescriptorRoot, ec) && !ec;
    const stdfs::path candidateLockWorkspace = candidateLockDescriptorRoot
        / candidateLockWorkspaceName;
    ec.clear();
    const bool candidateLockWorkspaceReady = candidateLockFixturesReady
        && stdfs::create_directory(candidateLockWorkspace, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(candidateLockWorkspace);
    Filesystem candidateLockFs(candidateLockDescriptorRoot.string());
    const bool candidateLockFsReady = candidateLockWorkspaceReady
        && candidateLockFs.initialize();
    ScopedDescriptorPressure candidateLockPressure;
    const bool candidateLockLimitApplied = candidateLockFsReady
        && candidateLockPressure.apply();
    const bool candidateLockLimitReached = candidateLockPressure.reachedLimit();
    bool candidateLockPending = candidateLockLimitApplied
        && candidateLockFs.maintenanceStep(32);
    const bool candidateLockWorkspacePreserved =
        stdfs::exists(candidateLockWorkspace);
    const bool candidateLockLimitRestored = candidateLockPressure.restore();
    std::size_t candidateLockMaintenanceSteps = 0;
    while (candidateLockPending && candidateLockMaintenanceSteps < 1000) {
        candidateLockPending = candidateLockFs.maintenanceStep(32);
        ++candidateLockMaintenanceSteps;
    }
    check(candidateLockWorkspaceReady && candidateLockFsReady
              && candidateLockLimitApplied && candidateLockLimitReached
              && candidateLockWorkspacePreserved && candidateLockLimitRestored
              && !candidateLockPending && candidateLockMaintenanceSteps < 1000
              && !stdfs::exists(candidateLockWorkspace),
          "startup cleanup retries a candidate after its parent lock hits descriptor exhaustion");

    const stdfs::path descriptorLimitRoot = hostRoot / "maintenance-descriptor-limit";
    stdfs::path descriptorLimitDeep = descriptorLimitRoot;
    ec.clear();
    bool descriptorLimitFixturesReady = stdfs::create_directory(descriptorLimitRoot, ec)
        && !ec;
    for (std::size_t depth = 0; descriptorLimitFixturesReady && depth < 96; ++depth) {
        const stdfs::path firstBranch = descriptorLimitDeep / "branch-a";
        const stdfs::path secondBranch = descriptorLimitDeep / "branch-b";
        ec.clear();
        descriptorLimitFixturesReady = stdfs::create_directory(firstBranch, ec) && !ec;
        ec.clear();
        descriptorLimitFixturesReady = descriptorLimitFixturesReady
            && stdfs::create_directory(secondBranch, ec) && !ec;
        ec.clear();
        stdfs::directory_iterator firstChild(descriptorLimitDeep, ec);
        descriptorLimitFixturesReady = descriptorLimitFixturesReady && !ec
            && firstChild != stdfs::directory_iterator{};
        if (descriptorLimitFixturesReady) descriptorLimitDeep = firstChild->path();
    }
    std::string descriptorWorkspaceName;
    descriptorLimitFixturesReady = descriptorLimitFixturesReady
        && monolith::detail::createAtomicTempTokenName(descriptorWorkspaceName);
    const stdfs::path descriptorStaleWorkspace = descriptorLimitDeep
        / descriptorWorkspaceName;
    ec.clear();
    descriptorLimitFixturesReady = descriptorLimitFixturesReady
        && stdfs::create_directory(descriptorStaleWorkspace, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(descriptorStaleWorkspace);

    Filesystem descriptorLimitFs(hostRoot.string());
    const bool descriptorLimitFsReady = descriptorLimitFixturesReady
        && descriptorLimitFs.initialize();
    ScopedDescriptorPressure descriptorPressure;
    const bool fileLimitApplied = descriptorLimitFsReady && descriptorPressure.apply();
    const bool fileDescriptorLimitReached = descriptorPressure.reachedLimit();
    bool descriptorMaintenancePending = false;
    std::size_t descriptorMaintenanceSteps = 0;
    if (fileLimitApplied) {
        descriptorMaintenancePending = descriptorLimitFs.maintenanceStep(32);
    }
    const bool fileLimitRestored = descriptorPressure.restore();
    if (fileLimitApplied) {
        while (descriptorMaintenancePending && descriptorMaintenanceSteps < 4000) {
            descriptorMaintenancePending = descriptorLimitFs.maintenanceStep(32);
            ++descriptorMaintenanceSteps;
        }
    }
    check(descriptorLimitFsReady && fileLimitApplied && fileDescriptorLimitReached
              && fileLimitRestored
              && !descriptorMaintenancePending && descriptorMaintenanceSteps < 4000
              && !stdfs::exists(descriptorStaleWorkspace),
          "startup cleanup resumes child directories after descriptor exhaustion clears");

    check(!fs.remove("/"), "non-recursive remove rejects the virtual root");
    check(fs.isDirectory("/"), "virtual root remains after a rejected remove");
    check(!fs.updateModifiedTime("/") && !fs.updateModifiedTime("/missing.txt"),
          "modified-time updates reject directories and missing paths");

    const stdfs::path entriesSweepParent = hostRoot / "entries-sweep";
    std::string entriesWorkspaceName;
    const bool entriesWorkspaceNameReady =
        monolith::detail::createAtomicTempTokenName(entriesWorkspaceName);
    const stdfs::path entriesStaleWorkspace = entriesSweepParent / entriesWorkspaceName;
    ec.clear();
    const bool entriesParentReady = stdfs::create_directory(entriesSweepParent, ec) && !ec;
    ec.clear();
    const bool entriesSweepFixturesReady = entriesParentReady && entriesWorkspaceNameReady
        && stdfs::create_directory(entriesStaleWorkspace, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(entriesStaleWorkspace)
        && writeFixture(entriesStaleWorkspace / "lease", "")
        && writeFixture(entriesStaleWorkspace / "ready", "")
        && writeFixture(entriesStaleWorkspace / "content", "abandoned");
    const auto entriesAfterSweep = fs.listEntries("/entries-sweep");
    check(entriesSweepFixturesReady && entriesAfterSweep.empty()
              && !stdfs::exists(entriesStaleWorkspace),
          "visiting a directory through typed listing reclaims abandoned save workspaces");

    const stdfs::path namesSweepParent = hostRoot / "names-sweep";
    const stdfs::path namesStaleWorkspace = namesSweepParent
        / (std::string(monolith::detail::atomicTempPreviousPrefix) + "listing");
    ec.clear();
    const bool namesParentReady = stdfs::create_directory(namesSweepParent, ec) && !ec;
    ec.clear();
    const bool namesSweepFixturesReady = namesParentReady
        && stdfs::create_directory(namesStaleWorkspace, ec) && !ec
        && writeFixture(namesStaleWorkspace / "lease", "")
        && writeFixture(namesStaleWorkspace / "ready", "")
        && writeFixture(namesStaleWorkspace / "content", "abandoned");
    const auto namesAfterSweep = fs.list("/names-sweep");
    check(namesSweepFixturesReady && namesAfterSweep.empty()
              && !stdfs::exists(namesStaleWorkspace),
          "visiting a directory through name listing reclaims abandoned save workspaces");

    const stdfs::path sharedCadenceParent = hostRoot / "shared-cadence";
    const stdfs::path firstSharedStaleWorkspace = sharedCadenceParent
        / (std::string(monolith::detail::atomicTempPreviousPrefix) + "first");
    const stdfs::path secondSharedStaleWorkspace = sharedCadenceParent
        / (std::string(monolith::detail::atomicTempPreviousPrefix) + "second");
    ec.clear();
    const bool sharedCadenceParentReady =
        stdfs::create_directory(sharedCadenceParent, ec) && !ec;
    ec.clear();
    const bool firstSharedStaleReady = sharedCadenceParentReady
        && stdfs::create_directory(firstSharedStaleWorkspace, ec) && !ec
        && writeFixture(firstSharedStaleWorkspace / "lease", "")
        && writeFixture(firstSharedStaleWorkspace / "ready", "")
        && writeFixture(firstSharedStaleWorkspace / "content", "first");
    const auto firstSharedListing = fs.listEntries("/shared-cadence");
    ec.clear();
    const bool secondSharedStaleReady = firstSharedStaleReady
        && stdfs::create_directory(secondSharedStaleWorkspace, ec) && !ec
        && writeFixture(secondSharedStaleWorkspace / "lease", "")
        && writeFixture(secondSharedStaleWorkspace / "ready", "")
        && writeFixture(secondSharedStaleWorkspace / "content", "second");
    const bool sharedCadenceWrite = secondSharedStaleReady
        && fs.writeFile("/shared-cadence/touch.txt", "saved");
    bool sharedCadenceRemainderSucceeded = sharedCadenceWrite;
    for (unsigned operation = 0;
         sharedCadenceRemainderSucceeded && operation < 30;
         ++operation) {
        fs.listEntries("/shared-cadence");
    }
    const bool heldBeforeCombinedInterval =
        stdfs::exists(secondSharedStaleWorkspace / "content");
    const auto sharedCadenceFinalListing = fs.list("/shared-cadence");
    check(firstSharedStaleReady && firstSharedListing.empty()
              && !stdfs::exists(firstSharedStaleWorkspace)
              && sharedCadenceRemainderSucceeded && heldBeforeCombinedInterval
              && !stdfs::exists(secondSharedStaleWorkspace)
              && sharedCadenceFinalListing.size() == 1,
          "directory listings and saves share one 32-operation cleanup cadence");

    check(fs.normalize("") == "/" && fs.normalize("////") == "/"
              && fs.normalize("../../alpha/../beta/") == "/beta",
          "path normalization preserves empty, repeated-slash, and root-clamping behavior");
    check(fs.join("", "") == "/"
              && fs.join("", "child") == "/child"
              && fs.join("/root", "/child") == "/root/child"
              && fs.join("/root", "../child") == "/child",
          "path joining preserves empty-root, slash-prefixed child, and parent traversal behavior");
    const std::vector<std::string> pathComponents = {
        "", ".", "..", "alpha", "two words", "\xC3\xA9" "clair"};
    const std::vector<std::string> pathPrefixes = {"", "/", "///"};
    bool matchesReference = true;
    for (const auto& prefix : pathPrefixes) {
        for (const auto& first : pathComponents) {
            for (const auto& second : pathComponents) {
                for (const auto& third : pathComponents) {
                    const std::string path = prefix + first + "/" + second
                        + "/" + third + "/";
                    if (fs.normalize(path) != referenceNormalize(path)) {
                        matchesReference = false;
                    }
                }
            }
        }
    }
    check(matchesReference,
          "single-pass path normalization matches the previous rules across component combinations");
    const std::vector<std::string> joinChildren = {
        "", ".", "..", "child", "../child", "/absolute", "///", "two words"};
    bool joinsMatchReference = true;
    for (const auto& prefix : pathPrefixes) {
        for (const auto& first : pathComponents) {
            for (const auto& second : pathComponents) {
                const std::string directory = prefix + first + "/" + second + "/";
                for (const auto& child : joinChildren) {
                    if (fs.join(directory, child) != referenceJoin(directory, child)) {
                        joinsMatchReference = false;
                    }
                }
            }
        }
    }
    check(joinsMatchReference,
          "direct path joining matches previous semantics for relative and slash-prefixed children");
    check(fs.isSameOrDescendant("/a", "/a/b")
              && fs.isSameOrDescendant("/a", "/a")
              && !fs.isSameOrDescendant("/a", "/ab")
              && fs.isSameOrDescendant("/", "../child"),
          "descendant checks respect component boundaries and the virtual root");
    std::vector<std::string> boundaryPaths = {
        "", "/", "///", "a", "a/b", "../x", "/a", "/a/b", "/ab",
        "/a/./b", "/a/b/../../a/child", "two words/file", "\xC3\xA9" "clair/item"};
    for (const auto& prefix : pathPrefixes) {
        for (const auto& first : pathComponents) {
            for (const auto& second : pathComponents) {
                boundaryPaths.push_back(prefix + first + "/" + second);
            }
        }
    }
    bool descendantChecksMatch = true;
    for (const auto& ancestor : boundaryPaths) {
        for (const auto& path : boundaryPaths) {
            if (fs.isSameOrDescendant(ancestor, path)
                != referenceIsSameOrDescendant(ancestor, path)) {
                descendantChecksMatch = false;
            }
        }
    }
    check(descendantChecksMatch,
          "direct descendant-boundary checks match previous semantics across generated paths");
    check(fs.normalize(std::string(65536, '/')) == "/",
          "normalization handles long redundant separator runs");

    monolith::test::ScopedTempDirectory outsideTemp("monolith-fs-outside");
    if (!outsideTemp) {
        std::cerr << "FAIL: could not create outside symlink target\n";
        return 1;
    }
    const stdfs::path outsideRoot = outsideTemp.path();
    check(stdfs::is_directory(outsideRoot), "create outside symlink target");
    {
        std::ofstream outsideFile(outsideRoot / "secret.txt");
        outsideFile << "outside";
    }
    const stdfs::path escapeLink = hostRoot / "escape";
    stdfs::create_directory_symlink(outsideRoot, escapeLink, ec);
    const bool escapeLinkReady = !ec;
    check(escapeLinkReady, "create outside symlink");
    const stdfs::path outsideNestedFileLink = outsideRoot / "nested-file-link";
    ec.clear();
    stdfs::create_symlink(outsideRoot / "secret.txt", outsideNestedFileLink, ec);
    const bool outsideNestedFileLinkReady = !ec;
    const stdfs::path outsideNestedDirectory = outsideRoot / "nested-target";
    ec.clear();
    const bool outsideNestedDirectoryReady =
        stdfs::create_directory(outsideNestedDirectory, ec) && !ec;
    const stdfs::path outsideNestedDirectoryLink = outsideRoot / "nested-directory-link";
    ec.clear();
    if (outsideNestedDirectoryReady) {
        stdfs::create_directory_symlink(outsideNestedDirectory,
                                        outsideNestedDirectoryLink, ec);
    }
    const bool outsideNestedDirectoryLinkReady = outsideNestedDirectoryReady && !ec;
    if (escapeLinkReady) {
        check(fs.toHostPath("/escape/secret.txt").empty(),
              "host path rejects symlink target outside root");
        check(!fs.exists("/escape") && !fs.isDirectory("/escape"),
              "outside symlink is not visible as a virtual entry");
        std::string escapedContent;
        check(!fs.readFile("/escape/secret.txt", escapedContent),
              "read rejects outside symlink target");
        check(!fs.readFileChunks("/escape/secret.txt", [](std::string_view) {
                  return true;
              }),
              "chunk reader rejects outside symlink target");
        const stdfs::path outsideFile = outsideRoot / "secret.txt";
        const auto outsideModifiedBefore = stdfs::last_write_time(outsideFile, ec);
        const bool outsideTimeRead = !ec;
        ec.clear();
        const bool outsideTimeRejected = !fs.updateModifiedTime("/escape/secret.txt");
        const auto outsideModifiedAfter = stdfs::last_write_time(outsideFile, ec);
        check(outsideTimeRead && outsideTimeRejected && !ec
                  && outsideModifiedAfter == outsideModifiedBefore,
              "modified-time updates cannot follow a symlink outside the virtual root");
        check(!fs.writeFile("/escape/new.txt", "blocked"),
              "write rejects outside symlink target");

        const stdfs::path movableOutsideLink = hostRoot / "movable-outside-link";
        const stdfs::path movedOutsideLink = hostRoot / "moved-outside-link";
        ec.clear();
        stdfs::create_symlink(outsideRoot / "secret.txt", movableOutsideLink, ec);
        const bool movableOutsideLinkReady = !ec;
        const bool outsideLinkMoved = movableOutsideLinkReady
            && fs.rename("/movable-outside-link", "/moved-outside-link");
        ec.clear();
        const auto movedOutsideLinkStatus = stdfs::symlink_status(movedOutsideLink, ec);
        const auto movedOutsideLinkTarget = stdfs::read_symlink(movedOutsideLink, ec);
        check(outsideLinkMoved && !stdfs::exists(movableOutsideLink)
                  && stdfs::is_symlink(movedOutsideLinkStatus) && !ec
                  && movedOutsideLinkTarget == outsideRoot / "secret.txt"
                  && stdfs::is_regular_file(outsideRoot / "secret.txt"),
              "rename moves an outside-target symlink entry without following it");

        const bool nestedFileDeleteRejected = !fs.remove("/escape/nested-file-link");
        const bool nestedFileRenameRejected =
            !fs.rename("/escape/nested-file-link", "/moved-nested-file-link");
        ec.clear();
        const bool nestedFileLinkPreserved =
            stdfs::is_symlink(stdfs::symlink_status(outsideNestedFileLink, ec)) && !ec;
        check(outsideNestedFileLinkReady && nestedFileDeleteRejected
                  && nestedFileRenameRejected
                  && nestedFileLinkPreserved,
              "remove and rename reject a final symlink reached through an outside parent");

        check(fs.writeFile("/outside-parent-move-source.txt", "keep source"),
              "write source for outside-parent rename rejection");
        const bool nestedDestinationRenameRejected =
            !fs.rename("/outside-parent-move-source.txt", "/escape/moved-file.txt");
        check(nestedDestinationRenameRejected
                  && fs.readFile("/outside-parent-move-source.txt") == "keep source"
                  && !stdfs::exists(outsideRoot / "moved-file.txt"),
              "rename rejects a destination reached through an outside parent");
        check(fs.remove("/outside-parent-move-source.txt"),
              "remove source after outside-parent rename rejection");

        const bool nestedDirectoryDeleteRejected =
            !fs.removeRecursive("/escape/nested-directory-link");
        ec.clear();
        const bool nestedDirectoryLinkPreserved =
            stdfs::is_symlink(stdfs::symlink_status(outsideNestedDirectoryLink, ec)) && !ec;
        check(outsideNestedDirectoryLinkReady && nestedDirectoryDeleteRejected
                  && nestedDirectoryLinkPreserved,
              "recursive remove rejects a final symlink reached through an outside parent");

        const auto rootEntries = fs.list("/");
        check(std::find(rootEntries.begin(), rootEntries.end(), "escape")
                  == rootEntries.end(),
              "directory listing hides outside symlink");
        std::ifstream outsideCheck(outsideRoot / "secret.txt");
        std::string outsideContent;
        std::getline(outsideCheck, outsideContent);
        check(outsideContent == "outside",
              "outside file remains untouched");
        const auto typedRootEntries = fs.listEntries("/");
        check(std::find_if(typedRootEntries.begin(), typedRootEntries.end(), [](const auto& entry) {
                  return entry.name == "escape";
              }) == typedRootEntries.end(),
              "typed directory listing hides outside symlink");
        check(fs.remove("/escape"),
              "remove unlinks a direct outside symlink entry");
        check(!stdfs::exists(escapeLink)
                  && stdfs::is_regular_file(outsideRoot / "secret.txt"),
              "removing an outside symlink preserves its target");
    }

    const stdfs::path recursiveEscapeLink = hostRoot / "recursive-escape";
    stdfs::create_directory_symlink(outsideRoot, recursiveEscapeLink, ec);
    check(!ec, "create direct outside symlink for recursive removal");
    if (!ec) {
        check(fs.removeRecursive("/recursive-escape"),
              "recursive remove unlinks a direct outside symlink entry");
        check(!stdfs::exists(recursiveEscapeLink)
                  && stdfs::is_regular_file(outsideRoot / "secret.txt"),
              "recursive symlink removal preserves its target");
    }

    check(fs.createDirectory("/outside-link-container"),
          "create directory containing an outside symlink");
    const stdfs::path nestedEscapeLink = hostRoot / "outside-link-container/escape";
    stdfs::create_directory_symlink(outsideRoot, nestedEscapeLink, ec);
    check(!ec, "create nested outside symlink");
    if (!ec) {
        check(fs.removeRecursive("/outside-link-container"),
              "recursive remove unlinks hidden outside symlinks");
        check(!fs.exists("/outside-link-container")
                  && stdfs::is_directory(outsideRoot)
                  && stdfs::is_regular_file(outsideRoot / "secret.txt"),
              "removing an outside symlink preserves its target");
    }

    check(fs.createDirectory("/symlink-target"), "create in-root symlink target");
    check(fs.writeFile("/symlink-target/keep.txt", "keep me"),
          "write in-root symlink target file");
    const stdfs::path internalLink = hostRoot / "internal-link";
    stdfs::create_directory_symlink(hostRoot / "symlink-target", internalLink, ec);
    check(!ec, "create in-root symlink");
    if (!ec) {
        const auto internalNames = fs.list("/");
        const auto internalEntries = fs.listEntries("/");
        const auto internalEntry = std::find_if(
            internalEntries.begin(), internalEntries.end(), [](const auto& entry) {
                return entry.name == "internal-link";
            });
        check(std::find(internalNames.begin(), internalNames.end(), "internal-link")
                  != internalNames.end()
                  && internalEntry != internalEntries.end()
                  && internalEntry->isDirectory,
              "directory listings retain in-root directory symlinks");

        const stdfs::path danglingListLink = hostRoot / "dangling-list-link";
        stdfs::create_symlink(hostRoot / "missing-list-target", danglingListLink, ec);
        check(!ec, "create in-root dangling listing symlink");
        if (!ec) {
            const auto danglingNames = fs.list("/");
            const auto danglingEntries = fs.listEntries("/");
            const auto danglingEntry = std::find_if(
                danglingEntries.begin(), danglingEntries.end(), [](const auto& entry) {
                    return entry.name == "dangling-list-link";
                });
            check(std::find(danglingNames.begin(), danglingNames.end(), "dangling-list-link")
                      != danglingNames.end()
                      && danglingEntry != danglingEntries.end()
                      && !danglingEntry->isDirectory,
                  "directory listings retain in-root dangling symlinks as files");
            check(fs.remove("/dangling-list-link"),
                  "remove in-root dangling listing symlink");
        }

        const stdfs::path internalFileLink = hostRoot / "internal-file-link";
        stdfs::create_symlink(hostRoot / "symlink-target/keep.txt", internalFileLink, ec);
        check(!ec, "create in-root file symlink");
        if (!ec) {
            const stdfs::path internalFile = hostRoot / "symlink-target/keep.txt";
            const auto oldInternalModified = stdfs::file_time_type::clock::now()
                - std::chrono::hours(24);
            stdfs::last_write_time(internalFile, oldInternalModified, ec);
            const bool agedInternalFile = !ec;
            ec.clear();
            const bool updatedInternalLink = fs.updateModifiedTime("/internal-file-link");
            const auto newInternalModified = stdfs::last_write_time(internalFile, ec);
            check(agedInternalFile && updatedInternalLink && !ec
                      && newInternalModified > oldInternalModified,
                  "modified-time updates follow in-root file symlinks");
            check(fs.remove("/internal-file-link"),
                  "remove unlinks an in-root file symlink entry");
            check(!stdfs::exists(internalFileLink) && fs.isFile("/symlink-target/keep.txt")
                      && fs.readFile("/symlink-target/keep.txt") == "keep me",
                  "removing an in-root file symlink preserves its target");
        }
        check(!fs.copyRecursive("/internal-link", "/copied-link"),
              "copy rejects a symlink source instead of traversing it");
        check(fs.writeFile("/internal-link/new.txt", "through link")
                  && fs.readFile("/symlink-target/new.txt") == "through link",
              "atomic writes preserve in-root symlink traversal");
        check(fs.removeRecursive("/internal-link"),
              "recursive remove deletes the symlink itself");
        check(!fs.exists("/internal-link") && fs.isFile("/symlink-target/keep.txt")
                  && fs.readFile("/symlink-target/keep.txt") == "keep me",
              "recursive symlink removal preserves the target tree");

        const stdfs::path renameLink = hostRoot / "rename-file-link";
        stdfs::create_symlink(hostRoot / "symlink-target/keep.txt", renameLink, ec);
        check(!ec, "create symlink source for rename");
        if (!ec) {
            check(fs.rename("/rename-file-link", "/renamed-file-link"),
                  "rename moves a symlink entry instead of its target");
            const stdfs::path renamedLink = hostRoot / "renamed-file-link";
            check(!stdfs::exists(renameLink) && stdfs::is_symlink(renamedLink)
                      && fs.isFile("/symlink-target/keep.txt")
                      && fs.readFile("/symlink-target/keep.txt") == "keep me",
                  "renaming a symlink preserves its target file");
            check(fs.remove("/renamed-file-link"),
                  "remove renamed symlink source fixture");
        }
    }

    check(fs.createDirectory("/copy-alias-source/nested"),
          "create source tree for physical copy-alias checks");
    check(fs.writeFile("/copy-alias-source/nested/keep.txt", "original"),
          "write source file for physical copy-alias checks");
    const stdfs::path rootCopyAlias = hostRoot / "copy-root-alias";
    stdfs::create_directory_symlink(
        hostRoot / "copy-alias-source/nested", rootCopyAlias, ec);
    check(!ec, "create destination alias into source tree");
    if (!ec) {
        check(!fs.copyRecursive("/copy-alias-source", "/copy-root-alias"),
              "recursive copy rejects a destination alias into its source");
        check(fs.readFile("/copy-alias-source/nested/keep.txt") == "original"
                  && !fs.exists("/copy-alias-source/nested/nested"),
              "root destination alias leaves source tree unchanged");
        check(fs.remove("/copy-root-alias"), "remove root destination alias");
    }

    check(fs.createDirectory("/copy-nested-alias-destination"),
          "create existing tree for nested destination alias check");
    const stdfs::path nestedCopyAlias =
        hostRoot / "copy-nested-alias-destination/nested";
    stdfs::create_directory_symlink(
        hostRoot / "copy-alias-source/nested", nestedCopyAlias, ec);
    check(!ec, "create nested destination alias into source tree");
    if (!ec) {
        check(!fs.copyRecursive(
                  "/copy-alias-source", "/copy-nested-alias-destination"),
              "recursive copy rejects a nested destination alias into its source");
        check(fs.readFile("/copy-alias-source/nested/keep.txt") == "original"
                  && !fs.exists("/copy-alias-source/nested/nested"),
              "nested destination alias leaves source tree unchanged");
        check(fs.remove("/copy-nested-alias-destination/nested"),
              "remove nested destination alias");
    }

    check(fs.writeFile("/copy-link-source.txt", "through destination link"),
          "write file for destination-link copy checks");
    check(fs.writeFile("/copy-link-target.txt", "old target"),
          "write target for destination-link copy check");
    const stdfs::path copyFileLink = hostRoot / "copy-file-link";
    stdfs::create_symlink(hostRoot / "copy-link-target.txt", copyFileLink, ec);
    check(!ec, "create in-root file destination symlink");
    if (!ec) {
        check(fs.copyRecursive("/copy-link-source.txt", "/copy-file-link")
                  && stdfs::is_symlink(stdfs::symlink_status(copyFileLink))
                  && fs.readFile("/copy-link-target.txt") == "through destination link",
              "file copy follows an in-root destination symlink without replacing it");
        check(fs.remove("/copy-file-link"), "remove copied file destination symlink");
    }

    const stdfs::path selfCopyLink = hostRoot / "copy-self-link";
    stdfs::create_symlink(hostRoot / "copy-link-source.txt", selfCopyLink, ec);
    check(!ec, "create source-file destination alias");
    if (!ec) {
        check(!fs.copyRecursive("/copy-link-source.txt", "/copy-self-link")
                  && fs.readFile("/copy-link-source.txt") == "through destination link",
              "file copy rejects a destination alias to the source file");
        check(fs.remove("/copy-self-link"), "remove source-file destination alias");
    }

    check(fs.createDirectory("/partial-src"), "create partial-copy source");
    check(fs.writeFile("/partial-src/a-good.txt", "keep"),
          "write partial-copy regular child");
    check(fs.writeFile("/partial-src/b-new.txt", "new child"),
          "write partial-copy new child");
    const stdfs::path partialLink = hostRoot / "partial-src/z-broken-link";
    stdfs::create_symlink(hostRoot / "missing-copy-target", partialLink, ec);
    check(!ec, "create partial-copy broken symlink child");
    if (!ec) {
        check(!fs.copyRecursive("/partial-src", "/partial-dst"),
              "recursive copy reports an unsupported child failure");
        check(!fs.exists("/partial-dst"),
              "failed recursive copy removes its partial new destination");
        check(fs.isFile("/partial-src/a-good.txt"),
              "failed recursive copy preserves the source tree");

        struct SourceEntry {
            std::string name;
            bool isDirectory = false;
        };
        auto sourceEntriesBeforeFailure = [&] {
            std::vector<SourceEntry> entries;
            for (const auto& entry : stdfs::directory_iterator(hostRoot / "partial-src")) {
                const std::string name = entry.path().filename().string();
                if (name == "z-broken-link") break;
                entries.push_back({name, entry.is_directory()});
            }
            return entries;
        };
        auto entriesBeforeFailure = sourceEntriesBeforeFailure();
        auto hasRollbackSetup = [](const std::vector<SourceEntry>& entries) {
            const auto fileCount = std::count_if(
                entries.begin(), entries.end(), [](const SourceEntry& entry) {
                    return !entry.isDirectory;
                });
            const bool hasNewDirectory = std::any_of(
                entries.begin(), entries.end(), [](const SourceEntry& entry) {
                    return entry.isDirectory;
                });
            return fileCount >= 3 && hasNewDirectory;
        };
        bool extraSourcesCreated = true;
        for (int candidate = 0; !hasRollbackSetup(entriesBeforeFailure) && candidate < 64;
             ++candidate) {
            const std::string suffix = std::to_string(candidate);
            extraSourcesCreated = fs.writeFile(
                "/partial-src/rollback-file-" + suffix, "candidate") && extraSourcesCreated;
            const std::string directoryName = "rollback-directory-" + suffix;
            extraSourcesCreated = fs.createDirectory(
                "/partial-src/" + directoryName) && extraSourcesCreated;
            extraSourcesCreated = fs.writeFile(
                "/partial-src/" + directoryName + "/child.txt", "nested candidate")
                && extraSourcesCreated;
            entriesBeforeFailure = sourceEntriesBeforeFailure();
        }
        std::vector<std::string> filesBeforeFailure;
        std::string directoryBeforeFailure;
        for (const auto& entry : entriesBeforeFailure) {
            if (entry.isDirectory && directoryBeforeFailure.empty()) {
                directoryBeforeFailure = entry.name;
            } else if (!entry.isDirectory) {
                filesBeforeFailure.push_back(entry.name);
            }
        }
        check(extraSourcesCreated && hasRollbackSetup(entriesBeforeFailure),
              "rollback fixture visits three regular files and a new directory before its rejected symlink");

        check(fs.createDirectory("/partial-merge-dst"),
              "create existing destination for copy rollback");
        const std::string overwrittenName = filesBeforeFailure.empty()
            ? "a-good.txt" : filesBeforeFailure.front();
        const std::string createdName = filesBeforeFailure.size() < 2
            ? "b-new.txt" : filesBeforeFailure[1];
        const std::string symlinkName = filesBeforeFailure.size() < 3
            ? "c-link-target.txt" : filesBeforeFailure[2];
        check(fs.writeFile("/partial-merge-dst/" + overwrittenName, "old content"),
              "write pre-existing file for copy rollback");
        check(fs.writeFile("/partial-merge-dst/keep.txt", "untouched"),
              "write unrelated file for copy rollback");
        const stdfs::path mergeDestination = hostRoot / "partial-merge-dst";
        const stdfs::path mergeExistingFile = mergeDestination / overwrittenName;
        const std::string symlinkTargetPath = "/partial-merge-dst/rollback-link-target.txt";
        const stdfs::path symlinkTargetHostPath = mergeDestination / "rollback-link-target.txt";
        check(fs.writeFile(symlinkTargetPath, "old symlink target"),
              "write in-root symlink target for copy rollback");
        ec.clear();
        stdfs::create_symlink(symlinkTargetHostPath,
                              mergeDestination / symlinkName, ec);
        check(!ec, "create in-root file symlink for copy rollback");
        const auto originalModifiedTime = stdfs::file_time_type::clock::now()
            - std::chrono::hours(24);
        stdfs::last_write_time(mergeExistingFile, originalModifiedTime, ec);
        const bool fileTimeSet = !ec;
        ec.clear();
        stdfs::last_write_time(symlinkTargetHostPath, originalModifiedTime, ec);
        const bool symlinkTargetTimeSet = !ec;
        ec.clear();
        stdfs::last_write_time(mergeDestination, originalModifiedTime, ec);
        const bool directoryTimeSet = !ec;
        check(fileTimeSet && symlinkTargetTimeSet && directoryTimeSet,
              "age existing file and directory before failed copy");

        check(!fs.copyRecursive("/partial-src", "/partial-merge-dst"),
              "recursive copy reports failure while merging into an existing tree");
        const auto restoredFileTime = stdfs::last_write_time(mergeExistingFile, ec);
        const bool fileTimeRestored = !ec && restoredFileTime == originalModifiedTime;
        ec.clear();
        const auto restoredSymlinkTargetTime = stdfs::last_write_time(
            symlinkTargetHostPath, ec);
        const bool symlinkTargetTimeRestored = !ec
            && restoredSymlinkTargetTime == originalModifiedTime;
        ec.clear();
        const auto restoredDirectoryTime = stdfs::last_write_time(mergeDestination, ec);
        const bool directoryTimeRestored = !ec
            && restoredDirectoryTime == originalModifiedTime;
        ec.clear();
        const auto rollbackSymlinkStatus = stdfs::symlink_status(
            mergeDestination / symlinkName, ec);
        const bool symlinkRestored = !ec && stdfs::is_symlink(rollbackSymlinkStatus);
        ec.clear();
        const auto mergeNames = fs.list("/partial-merge-dst");
        const bool noRollbackFilesRemain = std::none_of(
            mergeNames.begin(), mergeNames.end(), [](const std::string& name) {
                return name.find(".monolith-copy-rollback-") != std::string::npos;
            });
        check(fs.readFile("/partial-merge-dst/" + overwrittenName) == "old content"
                  && !fs.exists("/partial-merge-dst/" + createdName)
                  && !directoryBeforeFailure.empty()
                  && !fs.exists("/partial-merge-dst/" + directoryBeforeFailure)
                  && symlinkRestored
                  && fs.readFile(symlinkTargetPath) == "old symlink target"
                  && fs.readFile("/partial-merge-dst/keep.txt") == "untouched"
                  && fileTimeRestored && symlinkTargetTimeRestored
                  && directoryTimeRestored && noRollbackFilesRemain,
              "failed merge restores overwritten data and timestamps, removes new files, and preserves unrelated entries");
    }
    check(fs.removeRecursive("/partial-src"), "remove partial-copy source");

    const stdfs::path bulkSource = hostRoot / "bulk-src";
    stdfs::create_directories(bulkSource / "nested", ec);
    bool bulkFilesCreated = !ec;
    for (int i = 0; i < 96 && bulkFilesCreated; ++i) {
        const stdfs::path directory = i % 2 == 0 ? bulkSource : bulkSource / "nested";
        std::ofstream file(directory / ("item-" + std::to_string(i) + ".txt"),
                           std::ios::binary);
        file << "item-" << i;
        bulkFilesCreated = static_cast<bool>(file);
    }
    check(bulkFilesCreated, "create multi-entry recursive-copy source");
    const stdfs::path bulkOutsideLink = bulkSource / "outside-link";
    stdfs::create_symlink(outsideRoot / "secret.txt", bulkOutsideLink, ec);
    check(!ec, "create outside symlink in recursive-copy source");
    if (!ec && bulkFilesCreated) {
        const auto bulkNames = fs.list("/bulk-src");
        check(bulkNames.size() == 49
                  && std::find(bulkNames.begin(), bulkNames.end(), "item-0.txt")
                      != bulkNames.end()
                  && std::find(bulkNames.begin(), bulkNames.end(), "outside-link")
                      == bulkNames.end(),
              "wide regular-file listing skips only the external symlink");
        check(fs.copyRecursive("/bulk-src", "/bulk-dst"),
              "recursive copy walks a large directory tree");
        check(fs.listEntries("/bulk-dst").size() == 49
                  && fs.listEntries("/bulk-dst/nested").size() == 48,
              "recursive copy preserves every regular child without copying outside links");
        check(fs.readFile("/bulk-dst/item-0.txt") == "item-0"
                  && fs.readFile("/bulk-dst/nested/item-95.txt") == "item-95"
                  && !fs.exists("/bulk-dst/outside-link"),
              "recursive copy preserves nested contents and omits external symlinks");
        check(fs.removeRecursive("/bulk-src") && !fs.exists("/bulk-src")
                  && stdfs::is_regular_file(outsideRoot / "secret.txt"),
              "recursive deletion clears a wide tree without following external symlinks");
    }

    std::string deepSuffix;
    for (int i = 0; i < 128; ++i) deepSuffix += "/d";
    const std::string deepSource = "/deep-copy-source" + deepSuffix;
    const std::string deepDestination = "/deep-copy-destination" + deepSuffix;
    check(fs.createDirectory(deepSource), "create deeply nested copy source");
    check(fs.writeFile(fs.join(deepSource, "leaf.txt"), "deep content"),
          "write deeply nested copy leaf");
    check(fs.copyRecursive("/deep-copy-source", "/deep-copy-destination")
              && fs.readFile(fs.join(deepDestination, "leaf.txt")) == "deep content",
          "recursive copy preserves contents through a growing traversal stack");

    const stdfs::path deepInternalLink =
        hostRoot / deepSource.substr(1) / "z-internal-link";
    stdfs::create_symlink(hostRoot / "copy-link-source.txt", deepInternalLink, ec);
    check(!ec, "create in-root link at the bottom of a deep source tree");
    check(fs.createDirectory("/partial-deep-destination"),
          "create existing destination for deep rollback");
    check(!ec && !fs.copyRecursive("/deep-copy-source", "/partial-deep-destination")
              && fs.isDirectory("/partial-deep-destination")
              && fs.list("/partial-deep-destination").empty(),
          "failed deep copy removes its newly created subtree from an existing destination");

    check(fs.writeFile("/rename-source.txt", "keep source"),
          "write rename source for dangling-link coverage");
    check(fs.writeFile("/rename-existing-destination.txt", "keep destination"),
          "write existing regular rename destination");
    check(!fs.rename("/rename-source.txt", "/rename-existing-destination.txt"),
          "rename rejects an existing regular destination");
    check(fs.readFile("/rename-source.txt") == "keep source"
              && fs.readFile("/rename-existing-destination.txt") == "keep destination",
          "regular destination conflict preserves both files");
    check(fs.remove("/rename-existing-destination.txt"),
          "remove regular rename destination fixture");
    const stdfs::path danglingDestination = hostRoot / "dangling-destination";
    stdfs::create_symlink(hostRoot / "missing-target", danglingDestination, ec);
    check(!ec, "create dangling rename destination");
    if (!ec) {
        check(fs.exists("/dangling-destination"),
              "exists recognizes an in-root dangling symlink entry");
        check(!fs.rename("/rename-source.txt", "/dangling-destination"),
              "rename rejects an existing dangling symlink destination");
        check(fs.isFile("/rename-source.txt")
                  && stdfs::is_symlink(stdfs::symlink_status(danglingDestination)),
              "dangling destination and rename source remain intact");
    }

    monolith::test::ScopedTempDirectory fileRootTemp("monolith-fs-file-root");
    if (!fileRootTemp) {
        std::cerr << "FAIL: could not create invalid-root test directory\n";
        return 1;
    }
    const stdfs::path fileRoot = fileRootTemp.path() / "host-root-file";
    {
        std::ofstream blocker(fileRoot);
        blocker << "not a directory";
    }
    Filesystem invalidRoot(fileRoot.string());
    check(!invalidRoot.initialize(), "filesystem rejects a file as the host root");

    check(fs.createDirectory("/src"), "create /src");
    check(fs.createDirectory("/dst"), "create /dst");
    check(fs.createDirectory("/src/folder"), "create /src/folder");
    check(fs.writeFile("/src/a.txt", "alpha"), "write /src/a.txt");
    check(fs.writeFile("/src/b.txt", "bravo"), "write /src/b.txt");
    check(fs.writeFile("/src/notes.txt", "memo"), "write /src/notes.txt");
    check(fs.writeFile("/src/folder/c.txt", "charlie"), "write nested file");
    check(fs.writeFile("/src/Alpha.txt", "upper"), "write /src/Alpha.txt");
    check(fs.writeFile("/src/alpha.txt", "lower"), "write /src/alpha.txt");
    check(fs.writeFile("/src/other.dat", "zzz"), "write /src/other.dat");
    check(fs.writeFile("/src/empty.txt", ""), "write empty file");
    const std::string binaryContent("a\0b", 3);
    check(fs.writeFile("/src/binary.bin", binaryContent)
              && fs.readFile("/src/binary.bin") == binaryContent,
          "atomic virtual writes preserve embedded NUL bytes");
    check(fs.writeFile("/src/atomic.txt", "before"),
          "write initial atomic file");
    const auto atomicPermissions = stdfs::perms::owner_read
        | stdfs::perms::owner_write
        | stdfs::perms::group_read;
    stdfs::permissions(hostRoot / "src/atomic.txt", atomicPermissions,
                       stdfs::perm_options::replace, ec);
    check(!ec, "set atomic file permission fixture");
    {
        std::ofstream neighbor(hostRoot / "src/atomic.txt.tmp", std::ios::binary);
        neighbor << "important neighboring data";
    }
    check(fs.writeFile("/src/atomic.txt", "after")
              && fs.readFile("/src/atomic.txt") == "after"
              && fs.readFile("/src/atomic.txt.tmp") == "important neighboring data"
              && (stdfs::status(hostRoot / "src/atomic.txt").permissions()
                  == atomicPermissions)
              && !hasAtomicTempWorkspace(hostRoot / "src"),
          "atomic overwrite preserves a neighboring .tmp file and destination permissions");
    const auto specialAtomicPermissions = atomicPermissions
        | stdfs::perms::owner_exec | stdfs::perms::set_uid;
    ec.clear();
    stdfs::permissions(hostRoot / "src/atomic.txt", specialAtomicPermissions,
                       stdfs::perm_options::replace, ec);
    const bool specialPermissionsSet = !ec
        && stdfs::status(hostRoot / "src/atomic.txt").permissions()
            == specialAtomicPermissions;
    check(specialPermissionsSet, "set special atomic file permission fixture");
    const bool specialModeWrite = specialPermissionsSet
        && fs.writeFile("/src/atomic.txt", "after special mode");
    check(specialModeWrite
              && stdfs::status(hostRoot / "src/atomic.txt").permissions()
                  == specialAtomicPermissions,
          "atomic overwrite restores special permission bits after streaming");
    check(!fs.writeFileWithProducer("/src/atomic.txt", [](std::ostream& out) {
              out.write("partial", 7);
              return false;
          })
              && fs.readFile("/src/atomic.txt") == "after special mode"
              && fs.readFile("/src/atomic.txt.tmp") == "important neighboring data"
              && !hasAtomicTempWorkspace(hostRoot / "src"),
          "failed producer preserves both destination data and neighboring .tmp file");
    monolith::fs::FileStamp conditionalStamp;
    check(fs.fileStamp("/src/atomic.txt", conditionalStamp),
          "read conditional-write baseline stamp");
    const auto racedWriteResult = fs.writeFileWithProducerIfStampMatches(
        "/src/atomic.txt",
        [&fs](std::ostream& out) {
            if (!fs.writeFile("/src/atomic.txt", "external-during-write")) return false;
            out << "outer-write";
            return static_cast<bool>(out);
        },
        conditionalStamp);
    check(racedWriteResult == monolith::fs::ConditionalWriteResult::Conflict
              && fs.readFile("/src/atomic.txt") == "external-during-write"
              && !hasAtomicTempWorkspace(hostRoot / "src"),
          "conditional write preserves a host/other-process change during content generation");
    monolith::fs::FileStamp refreshedConditionalStamp;
    check(fs.fileStamp("/src/atomic.txt", refreshedConditionalStamp)
              && fs.writeFileWithProducerIfStampMatches(
                     "/src/atomic.txt",
                     [](std::ostream& out) {
                         out << "conditional-success";
                         return static_cast<bool>(out);
                     },
                     refreshedConditionalStamp)
                    == monolith::fs::ConditionalWriteResult::Written
              && fs.readFile("/src/atomic.txt") == "conditional-success",
          "conditional write replaces an unchanged version");
    const auto absentRaceResult = fs.writeFileWithProducerIfStampMatches(
        "/src/conditional-absent.txt",
        [&fs](std::ostream& out) {
            if (!fs.writeFile("/src/conditional-absent.txt", "created-during-write")) {
                return false;
            }
            out << "outer-create";
            return static_cast<bool>(out);
        },
        std::nullopt);
    check(absentRaceResult == monolith::fs::ConditionalWriteResult::Conflict
              && fs.readFile("/src/conditional-absent.txt") == "created-during-write"
              && !hasAtomicTempWorkspace(hostRoot / "src"),
          "conditional create preserves a destination created during content generation");
    check(fs.writeFileWithProducerIfStampMatches(
              "/src/conditional-new.txt",
              [](std::ostream& out) {
                  out << "created";
                  return static_cast<bool>(out);
              },
              std::nullopt) == monolith::fs::ConditionalWriteResult::Written
              && fs.readFile("/src/conditional-new.txt") == "created",
          "conditional write creates a target that was expected to be absent");
    bool publicationLockHeld = false;
    const stdfs::path publicationLockTarget = hostRoot / "src/publication-lock.txt";
    const bool publicationLockWrite = monolith::detail::writeAtomically(
        publicationLockTarget,
        [](std::ostream& out) {
            out << "published";
            return static_cast<bool>(out);
        },
        true,
        std::ios_base::out,
        [&publicationLockHeld, &hostRoot]() {
            monolith::detail::AtomicTempParentLock probe(hostRoot / "src", true);
            publicationLockHeld = !probe.locked()
                && (probe.error() == EWOULDBLOCK || probe.error() == EAGAIN);
            return true;
        });
    check(publicationLockWrite && publicationLockHeld
              && fs.readFile("/src/publication-lock.txt") == "published",
          "atomic publication holds the parent lock across final validation");
    bool targetAppearedDuringReplace = false;
    const stdfs::path noReplaceRaceTarget = hostRoot / "src/no-replace-race.txt";
    const bool noReplaceRaceWritten = monolith::detail::writeAtomically(
        noReplaceRaceTarget,
        [](std::ostream& out) {
            out << "outer";
            return static_cast<bool>(out);
        },
        true,
        std::ios_base::out,
        [&noReplaceRaceTarget]() {
            std::ofstream externalWriter(noReplaceRaceTarget);
            externalWriter << "concurrent";
            return static_cast<bool>(externalWriter);
        },
        true,
        &targetAppearedDuringReplace);
    check(!noReplaceRaceWritten && targetAppearedDuringReplace
              && fs.readFile("/src/no-replace-race.txt") == "concurrent"
              && !hasAtomicTempWorkspace(hostRoot / "src"),
          "no-replace publication preserves a target created after the final check");
    check(fs.writeFile("/src/conditional-link-a.txt", "target-a")
              && fs.writeFile("/src/conditional-link-b.txt", "target-b"),
          "write conditional symlink target fixtures");
    const stdfs::path conditionalLink = hostRoot / "src/conditional-link.txt";
    ec.clear();
    stdfs::create_symlink(hostRoot / "src/conditional-link-a.txt", conditionalLink, ec);
    check(!ec, "create conditional-write symlink fixture");
    if (!ec) {
        monolith::fs::FileStamp conditionalLinkStamp;
        check(fs.fileStamp("/src/conditional-link.txt", conditionalLinkStamp),
              "read conditional-write symlink target stamp");
        const auto symlinkRaceResult = fs.writeFileWithProducerIfStampMatches(
            "/src/conditional-link.txt",
            [&conditionalLink, &hostRoot, &ec](std::ostream& out) {
                stdfs::remove(conditionalLink, ec);
                if (ec) return false;
                stdfs::create_symlink(
                    hostRoot / "src/conditional-link-b.txt", conditionalLink, ec);
                if (ec) return false;
                out << "outer-write";
                return static_cast<bool>(out);
            },
            conditionalLinkStamp);
        check(symlinkRaceResult == monolith::fs::ConditionalWriteResult::Conflict
                  && fs.readFile("/src/conditional-link-a.txt") == "target-a"
                  && fs.readFile("/src/conditional-link-b.txt") == "target-b"
                  && !hasAtomicTempWorkspace(hostRoot / "src"),
              "conditional write rejects a symlink retarget during content generation");
    }
    const std::string streamedBinary("new\0bytes", 9);
    check(fs.writeFileWithProducer("/src/streamed.bin", [&streamedBinary](std::ostream& out) {
              out.write(streamedBinary.data(),
                        static_cast<std::streamsize>(streamedBinary.size()));
              return static_cast<bool>(out);
          })
              && fs.readFile("/src/streamed.bin") == streamedBinary,
          "content producer atomically writes binary stream output");
    const stdfs::path outsideTempTarget = outsideRoot / "atomic-temp-target.txt";
    {
        std::ofstream outsideTempFile(outsideTempTarget);
        outsideTempFile << "outside-before";
    }
    const stdfs::path atomicTempLink = hostRoot / "src/atomic.txt.tmp";
    stdfs::remove(atomicTempLink, ec);
    stdfs::create_symlink(outsideTempTarget, atomicTempLink, ec);
    check(!ec, "create neighboring atomic-temp-name symlink");
    if (!ec) {
        check(fs.writeFile("/src/atomic.txt", "symlink-neighbor-untouched"),
              "atomic write succeeds beside a neighboring symlink");
        std::ifstream outsideTempCheck(outsideTempTarget);
        std::string outsideTempContent;
        std::getline(outsideTempCheck, outsideTempContent);
        check(outsideTempContent == "outside-before",
              "neighboring symlink target remains untouched");
        check(fs.readFile("/src/atomic.txt") == "symlink-neighbor-untouched",
              "atomic write replaces its destination without replacing the neighboring link");
        check(stdfs::is_symlink(stdfs::symlink_status(atomicTempLink)),
              "neighboring symlink remains an entry");
        check(!hasAtomicTempWorkspace(hostRoot / "src"),
              "successful atomic write cleans its unique workspace");
        stdfs::remove(atomicTempLink, ec);
    }
    const stdfs::path blockedWritePath = hostRoot / "src/blocked-write.txt";
    stdfs::remove_all(blockedWritePath, ec);
    check(stdfs::create_directory(blockedWritePath),
          "create blocked write target");
    check(!fs.writeFile("/src/blocked-write.txt", "should fail")
              && stdfs::is_directory(blockedWritePath)
              && !hasAtomicTempWorkspace(hostRoot / "src"),
          "failed atomic write preserves the target and cleans up");
    check(fs.isFile("/src/empty.txt") && fs.readFile("/src/empty.txt").empty(),
          "read empty file without failure");
    std::string explicitRead;
    check(fs.readFile("/src/empty.txt", explicitRead) && explicitRead.empty(),
          "explicit read accepts empty file");
    check(fs.readFile("/src/a.txt", explicitRead) && explicitRead == "alpha",
          "explicit read returns file content");
    check(!fs.readFile("/src/missing.txt", explicitRead),
          "explicit read reports missing file");

    std::string chunkedContent(16 * 1024 * 2 + 7, 'q');
    chunkedContent[16 * 1024 - 1] = '\0';
    check(fs.writeFile("/src/chunked.bin", chunkedContent),
          "write chunked binary file");
    std::string streamedContent;
    size_t streamedChunks = 0;
    size_t largestChunk = 0;
    const bool streamed = fs.readFileChunks(
        "/src/chunked.bin",
        [&](std::string_view chunk) {
            ++streamedChunks;
            largestChunk = std::max(largestChunk, chunk.size());
            streamedContent.append(chunk);
            return true;
        });
    check(streamed && streamedContent == chunkedContent
              && streamedChunks == 3 && largestChunk <= 16 * 1024,
          "chunk reader preserves binary bytes in bounded pieces");
    std::string streamedTail;
    size_t tailChunks = 0;
    size_t largestTailChunk = 0;
    bool tailPrefixSkipped = false;
    const size_t tailByteLimit = 16 * 1024 + 8;
    const std::string expectedTail = chunkedContent.substr(
        chunkedContent.size() - tailByteLimit);
    check(fs.readFileTailChunks(
              "/src/chunked.bin", tailByteLimit, [&](std::string_view chunk) {
                  ++tailChunks;
                  largestTailChunk = std::max(largestTailChunk, chunk.size());
                  streamedTail.append(chunk);
                  return true;
              }, tailPrefixSkipped)
              && tailPrefixSkipped
              && streamedTail == expectedTail
              && tailChunks == 2
              && largestTailChunk <= 16 * 1024,
          "tail chunk reader seeks to a bounded binary suffix with embedded NUL data");
    std::string completeTail;
    bool completeTailPrefixSkipped = true;
    check(fs.readFileTailChunks(
              "/src/chunked.bin", chunkedContent.size(), [&](std::string_view chunk) {
                  completeTail.append(chunk);
                  return true;
              }, completeTailPrefixSkipped)
              && !completeTailPrefixSkipped
              && completeTail == chunkedContent,
          "tail chunk reader reports when no prefix was omitted");
    std::string firstChunk;
    check(fs.readFileChunks("/src/chunked.bin", [&](std::string_view chunk) {
              firstChunk.assign(chunk);
              return false;
          })
              && firstChunk == chunkedContent.substr(0, 16 * 1024),
          "chunk reader treats an early consumer stop as success");
    size_t emptyFileChunks = 0;
    check(fs.readFileChunks("/src/empty.txt", [&](std::string_view) {
              ++emptyFileChunks;
              return true;
          })
              && emptyFileChunks == 0,
          "chunk reader distinguishes an empty file from a read failure");
    check(!fs.readFileChunks("/src/missing.txt", [](std::string_view) {
              return true;
          }),
          "chunk reader reports a missing file");
    bool missingTailPrefixSkipped = false;
    check(!fs.readFileTailChunks("/src/missing.txt", 16, [](std::string_view) {
              return true;
          }, missingTailPrefixSkipped),
          "tail chunk reader reports a missing file");

    std::uint64_t emptySize = 99;
    check(fs.fileSize("/src/empty.txt", emptySize) && emptySize == 0,
          "empty file reports zero bytes");

    // Multi-item copy into a destination folder (paste).
    const int copied = fs.copyItemsInto(
        {"/src/a.txt", "/src/b.txt", "/src/folder"},
        "/dst");
    check(copied == 3, "copyItemsInto copied three sources");
    check(fs.isFile("/dst/a.txt") && fs.readFile("/dst/a.txt") == "alpha",
          "pasted a.txt content");
    check(fs.isFile("/dst/b.txt") && fs.readFile("/dst/b.txt") == "bravo",
          "pasted b.txt content");
    check(fs.isDirectory("/dst/folder"), "pasted folder");
    check(fs.isFile("/dst/folder/c.txt") && fs.readFile("/dst/folder/c.txt") == "charlie",
          "pasted nested file via copyRecursive");
    check(fs.copyRecursive("/src/chunked.bin", "/dst/chunked-copy.bin")
              && fs.readFile("/dst/chunked-copy.bin") == chunkedContent,
          "recursive copy streams binary content across chunk boundaries");
    check(fs.writeFile("/dst/chunked-overwrite.bin", "old")
              && fs.copyRecursive("/src/chunked.bin", "/dst/chunked-overwrite.bin")
              && fs.readFile("/dst/chunked-overwrite.bin") == chunkedContent,
          "streamed recursive copy atomically replaces an existing file");
    check(fs.copyRecursive("/src/empty.txt", "/dst/empty.txt"),
          "copy empty file");
    std::uint64_t copiedEmptySize = 99;
    check(fs.fileSize("/dst/empty.txt", copiedEmptySize) && copiedEmptySize == 0,
          "copied empty file remains zero bytes");

    check(fs.copyRecursive("/src/notes.txt", "/new/tree/notes.txt"),
          "copy file creates missing destination parents");
    check(fs.readFile("/new/tree/notes.txt") == "memo",
          "direct file copy preserves content below a new destination tree");

    check(fs.writeFile("/src/move.txt", "move me"), "write move source");
    check(fs.writeFile("/src/conflict.txt", "keep source"), "write conflicting source");
    check(fs.writeFile("/dst/conflict.txt", "keep destination"), "write conflicting destination");
    const int moved = fs.moveItemsInto({"/src/move.txt", "/src/conflict.txt"}, "/dst");
    check(moved == 1, "moveItemsInto moves only non-conflicting source");
    check(!fs.exists("/src/move.txt") && fs.readFile("/dst/move.txt") == "move me",
          "moved source leaves destination content");
    check(fs.readFile("/src/conflict.txt") == "keep source"
              && fs.readFile("/dst/conflict.txt") == "keep destination",
          "conflicting source and destination remain intact");
    const stdfs::path batchMoveLink = hostRoot / "src/external-link";
    const stdfs::path batchMovedLink = hostRoot / "dst/external-link";
    ec.clear();
    stdfs::create_symlink(outsideRoot / "secret.txt", batchMoveLink, ec);
    const bool batchMoveLinkReady = !ec;
    const int movedOutsideLink = batchMoveLinkReady
        ? fs.moveItemsInto({"/src/external-link"}, "/dst")
        : 0;
    ec.clear();
    const auto batchMovedLinkStatus = stdfs::symlink_status(batchMovedLink, ec);
    const auto batchMovedLinkTarget = stdfs::read_symlink(batchMovedLink, ec);
    check(movedOutsideLink == 1 && !stdfs::exists(batchMoveLink)
              && stdfs::is_symlink(batchMovedLinkStatus) && !ec
              && batchMovedLinkTarget == outsideRoot / "secret.txt"
              && stdfs::is_regular_file(outsideRoot / "secret.txt"),
          "moveItemsInto moves a hidden outside-target symlink entry safely");

    // Same-folder / existing dest should not overwrite.
    const int copiedAgain = fs.copyItemsInto({"/src/a.txt"}, "/dst");
    check(copiedAgain == 0, "copyItemsInto skips existing dest name");
    check(fs.readFile("/dst/a.txt") == "alpha", "existing dest unchanged");

    // Rename rejects names containing '/'.
    check(!Filesystem::isValidEntryName("foo/bar"), "isValidEntryName rejects slash");
    check(!Filesystem::isValidEntryName(""), "isValidEntryName rejects empty");
    check(Filesystem::isValidEntryName("ok.txt"), "isValidEntryName accepts simple name");
    check(fs.renameEntry("/src", "a.txt", "renamed.txt"), "renameEntry valid name");
    check(fs.isFile("/src/renamed.txt"), "renamed file exists");
    check(!fs.renameEntry("/src", "renamed.txt", "foo/bar"), "renameEntry rejects slash name");
    check(fs.isFile("/src/renamed.txt"), "slash rename left original in place");
    check(!fs.exists("/src/foo"), "slash rename did not create nested path");
    check(!fs.exists("/src/foo/bar"), "slash rename did not write nested file");

    // A directory cannot be moved into its own subtree or replace the virtual root.
    check(!fs.rename("/src", "/src/folder/moved"),
          "rename rejects moving a directory into its descendant");
    check(fs.isDirectory("/src") && fs.isFile("/src/folder/c.txt")
              && !fs.exists("/src/folder/moved"),
          "descendant rename leaves the source tree intact");
    check(!fs.rename("/", "/reparented"), "rename rejects moving the virtual root");

    // Listing filter/search by name.
    auto listed = fs.listEntries("/src");
    auto folderIt = std::find_if(listed.begin(), listed.end(), [](const auto& entry) {
        return entry.name == "folder";
    });
    auto upperIt = std::find_if(listed.begin(), listed.end(), [](const auto& entry) {
        return entry.name == "Alpha.txt";
    });
    auto lowerIt = std::find_if(listed.begin(), listed.end(), [](const auto& entry) {
        return entry.name == "alpha.txt";
    });
    check(folderIt != listed.end() && upperIt != listed.end() && lowerIt != listed.end()
              && folderIt < upperIt && upperIt < lowerIt,
          "listEntries keeps directories first and sorts names case-insensitively");
    auto notes = Filesystem::filterEntries(listed, "note");
    check(notes.size() == 1 && notes.front().name == "notes.txt",
          "filterEntries matches notes.txt");
    auto none = Filesystem::filterEntries(listed, "no-such-name");
    check(none.empty(), "filterEntries empty on miss");
    auto all = Filesystem::filterEntries(listed, "");
    check(all.size() == listed.size(), "empty query returns all entries");
    check(Filesystem::entryNameMatches("Notes.TXT", "note"),
          "entryNameMatches is case-insensitive");
    check(Filesystem::entryNameMatches("Alpha.TXT", "PHA."),
          "entryNameMatches finds a mixed-case substring inside a name");
    check(!Filesystem::entryNameMatches("other.dat", "note"),
          "entryNameMatches rejects non-matching name");
    check(!Filesystem::entryNameMatches("note", "notes"),
          "entryNameMatches rejects a query longer than the name");
    const std::vector<Filesystem::DirEntry> mixedCaseEntries = {
        {"ALPHA.txt", false}, {"notes.TXT", false}, {"other.dat", false},
    };
    const auto mixedCaseMatches = Filesystem::filterEntries(mixedCaseEntries, "tXt");
    check(mixedCaseMatches.size() == 2
              && mixedCaseMatches[0].name == "ALPHA.txt"
              && mixedCaseMatches[1].name == "notes.TXT",
          "filterEntries matches mixed-case queries without changing entry order");
    const auto mixedCaseIndices = Filesystem::filterEntryIndices(mixedCaseEntries, "tXt");
    check(mixedCaseIndices == std::vector<std::size_t>{0, 1},
          "filterEntryIndices matches without copying entries and preserves source order");
    std::vector<std::size_t> narrowingIndices{0, 1, 2};
    Filesystem::filterEntryIndicesInPlace(mixedCaseEntries, "tXt", narrowingIndices);
    check(narrowingIndices == std::vector<std::size_t>{0, 1},
          "in-place filtering narrows candidate indices while preserving order");
    Filesystem::filterEntryIndicesInPlace(mixedCaseEntries, "PHA.", narrowingIndices);
    check(narrowingIndices == std::vector<std::size_t>{0},
          "successive in-place filtering keeps only candidates matching the narrower query");
    check(Filesystem::filterEntryIndices(mixedCaseEntries, "").empty(),
          "filterEntryIndices uses an empty result for the unfiltered identity view");

    if (failures == 0) {
        std::cout << "ALL FS ROADMAP TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
