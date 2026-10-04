#include "../src/detail/AtomicFile.hpp"
#include "TestTempDir.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

static bool writeFixture(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
    return static_cast<bool>(out);
}

static bool writeOwnerMarker(const fs::path& directory) {
    return writeFixture(directory / monolith::detail::atomicTempOwnerName,
                        monolith::detail::atomicTempOwnerMarker);
}

static fs::path workspacePath(const fs::path& parent,
                              const fs::path& target,
                              unsigned long long sequence,
                              const char* prefix = monolith::detail::atomicTempPrefix) {
    const auto hash = std::hash<std::string>{}(target.filename().string());
    return parent / (std::string(prefix)
                     + std::to_string(hash) + "-" + std::to_string(sequence));
}

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (ok) {
            std::cout << "ok: " << message << '\n';
        } else {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };

    monolith::test::ScopedTempDirectory temp("monolith-atomic-file");
    if (!temp) {
        std::cerr << "FAIL: could not create atomic-file test directory\n";
        return 1;
    }
    const fs::path parent = temp.path();
    std::error_code ec;
    check(monolith::detail::pathBasenameView(fs::path("relative/workspace")) == "workspace"
              && monolith::detail::pathBasenameView(fs::path("relative/workspace/"))
                  == "workspace"
              && monolith::detail::pathBasenameView(fs::path("/")).empty(),
          "atomic workspace basename views handle relative, trailing-separator, and root paths");

    {
        monolith::detail::AtomicTempParentLock heldLock(parent);
        monolith::detail::AtomicTempParentLock competingLock(parent, true);
        check(heldLock.locked() && !competingLock.locked(),
              "workspace setup and sweeping share an exclusive parent lock");
    }

    const fs::path aliasTarget = parent / "alias-target";
    const fs::path aliasPath = parent / "alias-path";
    ec.clear();
    const bool aliasTargetReady = fs::create_directory(aliasTarget, ec) && !ec;
    ec.clear();
    if (aliasTargetReady) fs::create_directory_symlink(aliasTarget, aliasPath, ec);
    const bool aliasFixturesReady = aliasTargetReady && !ec;
    const bool firstAliasSweep = aliasFixturesReady
        && monolith::detail::shouldSweepAtomicTempParent(aliasTarget);
    const bool secondAliasSweep = aliasFixturesReady
        && monolith::detail::shouldSweepAtomicTempParent(aliasPath);
    bool aliasCadenceHeld = aliasFixturesReady && !secondAliasSweep;
    for (unsigned long long write = 2;
         aliasFixturesReady && write < monolith::detail::atomicTempSweepInterval;
         ++write) {
        const auto& spelling = write % 2 == 0 ? aliasTarget : aliasPath;
        aliasCadenceHeld = aliasCadenceHeld
            && !monolith::detail::shouldSweepAtomicTempParent(spelling);
    }
    const bool aliasIntervalSweep = aliasFixturesReady
        && monolith::detail::shouldSweepAtomicTempParent(aliasTarget);
    check(aliasFixturesReady && firstAliasSweep && aliasCadenceHeld && aliasIntervalSweep,
          "symlink aliases share one destination-parent sweep cadence");

    const fs::path fifoMarkerDirectory = parent
        / (std::string(monolith::detail::atomicTempPrefix) + "fifo-owner");
    const fs::path fifoLeaseDirectory = parent
        / (std::string(monolith::detail::atomicTempPrefix) + "fifo-lease");
    ec.clear();
    const bool fifoDirectoryReady = fs::create_directory(fifoMarkerDirectory, ec) && !ec;
    const fs::path fifoOwnerPath = fifoMarkerDirectory / monolith::detail::atomicTempOwnerName;
    ec.clear();
    const bool fifoLeaseDirectoryReady = fs::create_directory(fifoLeaseDirectory, ec) && !ec;
    const fs::path fifoLeasePath = fifoLeaseDirectory / "lease";
    const bool fifoFixturesReady = fifoDirectoryReady
        && ::mkfifo(fifoOwnerPath.c_str(), S_IRUSR | S_IWUSR) == 0
        && writeFixture(fifoMarkerDirectory / "notes.txt", "keep this directory")
        && fifoLeaseDirectoryReady
        && writeOwnerMarker(fifoLeaseDirectory)
        && ::mkfifo(fifoLeasePath.c_str(), S_IRUSR | S_IWUSR) == 0
        && writeFixture(fifoLeaseDirectory / "ready", "")
        && writeFixture(fifoLeaseDirectory / "notes.txt", "keep this lease lookalike");
    bool fifoSweepFinished = false;
    if (fifoFixturesReady) {
        const pid_t child = ::fork();
        if (child == 0) {
            ::alarm(2);
            monolith::detail::scavengeAtomicTempDirectories(parent);
            const bool preserved = fs::exists(fifoOwnerPath)
                && fs::exists(fifoMarkerDirectory / "notes.txt")
                && fs::exists(fifoLeasePath)
                && fs::exists(fifoLeaseDirectory / "notes.txt");
            _exit(preserved ? 0 : 1);
        }
        int childStatus = 0;
        const pid_t waited = child > 0 ? ::waitpid(child, &childStatus, 0) : -1;
        fifoSweepFinished = waited == child && WIFEXITED(childStatus)
            && WEXITSTATUS(childStatus) == 0;
    }
    check(fifoFixturesReady && fifoSweepFinished,
          "FIFO owner and lease lookalikes cannot block cleanup or lose user data");

    const fs::path orphanPath = parent
        / (std::string(monolith::detail::atomicTempPrefix)
           + "0123456789abcdef0123456789abcdef");
    const fs::path tokenLookalikePath = parent
        / (std::string(monolith::detail::atomicTempPrefix)
           + "fedcba9876543210fedcba9876543210");
    ec.clear();
    const bool orphanFixturesReady = fs::create_directory(orphanPath, ec) && !ec
        && fs::create_directory(tokenLookalikePath, ec) && !ec
        && writeFixture(tokenLookalikePath / "notes.txt", "keep this directory");
    if (orphanFixturesReady) monolith::detail::scavengeAtomicTempDirectories(parent);
    check(orphanFixturesReady && !fs::exists(orphanPath)
              && fs::exists(tokenLookalikePath / "notes.txt"),
          "cleanup reclaims empty pre-marker orphans but preserves nonempty token lookalikes");

    const fs::path sweepBatchParent = parent / "sweep-batch";
    ec.clear();
    bool sweepBatchFixturesReady = fs::create_directory(sweepBatchParent, ec) && !ec;
    constexpr std::size_t sweepBatchSize = 128;
    for (std::size_t index = 0; sweepBatchFixturesReady && index < sweepBatchSize; ++index) {
        const fs::path workspace = sweepBatchParent
            / (std::string(monolith::detail::atomicTempPreviousPrefix)
               + std::to_string(index));
        ec.clear();
        sweepBatchFixturesReady = fs::create_directory(workspace, ec) && !ec
            && writeFixture(workspace / "lease", "")
            && writeFixture(workspace / "ready", "")
            && writeFixture(workspace / "content", "stale snapshot");
    }
    for (std::size_t index = 0; sweepBatchFixturesReady && index < sweepBatchSize; ++index) {
        sweepBatchFixturesReady = writeFixture(
            sweepBatchParent / ("ordinary-" + std::to_string(index) + ".txt"),
            "user data");
    }
    bool sweepBatchCompleted = false;
    bool sweepBatchLockRetained = false;
    if (sweepBatchFixturesReady) {
        monolith::detail::AtomicTempParentLock sweepBatchLock(sweepBatchParent);
        if (sweepBatchLock.locked()) {
            sweepBatchCompleted = monolith::detail::scavengeAtomicTempDirectoriesLocked(
                sweepBatchParent, sweepBatchLock);
            sweepBatchLockRetained = sweepBatchLock.locked();
        }
    }
    std::size_t sweepBatchRemaining = 0;
    std::size_t ordinaryEntriesRemaining = 0;
    if (sweepBatchFixturesReady) {
        for (const auto& entry : fs::directory_iterator(sweepBatchParent)) {
            const std::string name = entry.path().filename().string();
            if (name.starts_with(monolith::detail::atomicTempPreviousPrefix)) {
                ++sweepBatchRemaining;
            } else if (name.starts_with("ordinary-")) {
                ++ordinaryEntriesRemaining;
            }
        }
    }
    check(sweepBatchCompleted && sweepBatchLockRetained && sweepBatchRemaining == 0
              && ordinaryEntriesRemaining == sweepBatchSize,
          "a sweep reuses the setup lock while reclaiming stale workspaces and preserving siblings");

    const fs::path failedSetupPath = parent
        / (std::string(monolith::detail::atomicTempPrefix)
           + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const fs::path emptySetupPath = parent
        / (std::string(monolith::detail::atomicTempPrefix)
           + "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    ec.clear();
    const bool setupRollbackFixturesReady = fs::create_directory(failedSetupPath, ec) && !ec
        && writeFixture(failedSetupPath / "notes.txt", "preserve this entry")
        && fs::create_directory(emptySetupPath, ec) && !ec;
    const bool setupRollbackPreservesUnexpectedEntry = setupRollbackFixturesReady
        && !monolith::detail::removeAtomicTempDirectoryIfEmpty(failedSetupPath)
        && fs::exists(failedSetupPath / "notes.txt");
    const bool setupRollbackRemovesEmptyDirectory = setupRollbackFixturesReady
        && monolith::detail::removeAtomicTempDirectoryIfEmpty(emptySetupPath)
        && !fs::exists(emptySetupPath);
    check(setupRollbackPreservesUnexpectedEntry && setupRollbackRemovesEmptyDirectory,
          "unpublished workspace rollback removes empty directories but preserves unexpected entries");

    const fs::path markerParent = parent / "marker-check";
    ec.clear();
    const bool markerParentReady = fs::create_directory(markerParent, ec) && !ec;
    bool markerObservedDuringSave = false;
    bool markerPublishedAtomically = false;
    bool workspaceNameValidated = false;
    bool workspacePermissionsPrivate = false;
    const bool markerWrite = markerParentReady
        && monolith::detail::writeTextAtomically(
            markerParent / "record.txt",
            [&](std::ostream& out) {
                for (const auto& entry : fs::directory_iterator(markerParent)) {
                    if (!entry.path().filename().string().starts_with(
                            monolith::detail::atomicTempPrefix)) {
                        continue;
                    }
                    const fs::path ownerPath = entry.path()
                        / monolith::detail::atomicTempOwnerName;
                    std::error_code ownerError;
                    const auto ownerStatus = fs::symlink_status(ownerPath, ownerError);
                    if (!ownerError && fs::is_symlink(ownerStatus)) {
                        const auto ownerTarget = fs::read_symlink(ownerPath, ownerError);
                        markerPublishedAtomically = !ownerError
                            && ownerTarget == monolith::detail::atomicTempOwnerMarker;
                    }
                    workspaceNameValidated = monolith::detail::hasAtomicTempTokenName(
                        entry.path());
                    struct stat workspaceStatus {};
                    workspacePermissionsPrivate =
                        ::stat(entry.path().c_str(), &workspaceStatus) == 0
                        && (workspaceStatus.st_mode & (S_IRWXG | S_IRWXO)) == 0;
                    markerObservedDuringSave = monolith::detail::hasAtomicTempOwnerMarker(
                        entry.path())
                        && markerPublishedAtomically
                        && fs::is_regular_file(entry.path() / "ready")
                        && fs::is_regular_file(entry.path() / "lease");
                    break;
                }
                out << "owned";
            });
    check(markerWrite && markerObservedDuringSave && workspaceNameValidated
              && workspacePermissionsPrivate,
          "new workspaces use validated random names and private permissions before content");

    const fs::path target = parent / "settings.txt";
    const auto stalePath = workspacePath(parent, target, 7);
    fs::create_directory(stalePath, ec);
    std::string currentWorkspaceName;
    const bool currentWorkspaceNameReady =
        monolith::detail::createAtomicTempTokenName(currentWorkspaceName);
    const fs::path currentStalePath = parent / currentWorkspaceName;
    ec.clear();
    const bool currentStaleFixturesReady = currentWorkspaceNameReady
        && fs::create_directory(currentStalePath, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(currentStalePath)
        && writeFixture(currentStalePath / "lease", "")
        && writeFixture(currentStalePath / "ready", "")
        && writeFixture(currentStalePath / "content", "current partial snapshot");
    const bool staleFixturesReady = !ec
        && writeOwnerMarker(stalePath)
        && writeFixture(stalePath / "lease", "")
        && writeFixture(stalePath / "ready", "")
        && writeFixture(stalePath / "content", "partial snapshot");
    const fs::path userIncompletePath = parent
        / (std::string(monolith::detail::atomicTempPrefix) + "user-notes");
    ec.clear();
    const bool userIncompleteReady = fs::create_directory(userIncompletePath, ec) && !ec
        && writeFixture(userIncompletePath / "notes.txt", "keep this directory");
    const fs::path userMarkedPath = parent
        / (std::string(monolith::detail::atomicTempPrefix) + "marked-user-data");
    ec.clear();
    const bool userMarkedReady = fs::create_directory(userMarkedPath, ec) && !ec
        && writeFixture(userMarkedPath / "lease", "")
        && writeFixture(userMarkedPath / "ready", "")
        && writeFixture(userMarkedPath / monolith::detail::atomicTempOwnerName,
                        "not Monolith's marker")
                        && writeFixture(userMarkedPath / "notes.txt", "keep this too");
    const fs::path symlinkMarkedLookalikePath = parent
        / (std::string(monolith::detail::atomicTempPrefix) + "symlink-lookalike");
    ec.clear();
    const bool symlinkMarkedLookalikeReady =
        fs::create_directory(symlinkMarkedLookalikePath, ec) && !ec
        && monolith::detail::createAtomicTempOwnerMarker(symlinkMarkedLookalikePath)
        && writeFixture(symlinkMarkedLookalikePath / "lease", "")
        && writeFixture(symlinkMarkedLookalikePath / "ready", "")
        && writeFixture(symlinkMarkedLookalikePath / "content", "user snapshot");
    const bool staleWrite = staleFixturesReady && currentStaleFixturesReady
        && userIncompleteReady && userMarkedReady && symlinkMarkedLookalikeReady
        && monolith::detail::writeTextAtomically(
            target, [](std::ostream& out) { out << "recovered"; });
    check(staleWrite && !fs::exists(stalePath) && !fs::exists(currentStalePath)
              && fs::exists(target) && fs::file_size(target, ec) == 9,
          "marked legacy and current-format workspaces are reclaimed after an interrupted save");
    check(userIncompleteReady && userMarkedReady
              && fs::exists(userIncompletePath / "notes.txt")
              && fs::exists(userMarkedPath / "notes.txt"),
          "lookalike directories without a valid ownership marker survive cleanup");
    check(symlinkMarkedLookalikeReady
              && fs::exists(symlinkMarkedLookalikePath / "content"),
          "a symlink owner marker without a random workspace token cannot authorize cleanup");

    const fs::path retryParent = parent / "retry-after-sweep-failure";
    const bool retryWasDue = monolith::detail::shouldSweepAtomicTempParent(retryParent);
    const bool missingParentSweepReported =
        !monolith::detail::scavengeAtomicTempDirectories(retryParent);
    ec.clear();
    const bool retryParentCreated = fs::create_directory(retryParent, ec) && !ec;
    const bool retryTriggeredImmediately = retryParentCreated
        && monolith::detail::shouldSweepAtomicTempParent(retryParent);
    const bool retrySweepCompleted = retryTriggeredImmediately
        && monolith::detail::scavengeAtomicTempDirectories(retryParent);
    const bool normalCadenceRestored = retrySweepCompleted
        && !monolith::detail::shouldSweepAtomicTempParent(retryParent);
    check(retryWasDue && missingParentSweepReported && retryParentCreated
              && retryTriggeredImmediately && retrySweepCompleted && normalCadenceRestored,
          "a failed sweep retries on the next write, then resumes its normal cadence");

    constexpr unsigned long long activeSequence = 12;
    const fs::path activePath = workspacePath(parent, target, activeSequence);
    ec.clear();
    const bool activeFixtureReady = fs::create_directory(activePath, ec) && !ec
        && writeOwnerMarker(activePath)
        && writeFixture(activePath / "lease", "")
        && writeFixture(activePath / "ready", "")
        && writeFixture(activePath / "content", "active partial snapshot");
    const int activeLeaseFd = activeFixtureReady
        ? ::open((activePath / "lease").c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW)
        : -1;
    const bool activeLockHeld = activeLeaseFd >= 0
        && ::flock(activeLeaseFd, LOCK_EX | LOCK_NB) == 0;
    if (activeFixtureReady && activeLockHeld) {
        monolith::detail::scavengeAtomicTempDirectories(parent);
    }
    check(activeFixtureReady && activeLockHeld
              && fs::exists(activePath / "content")
              && fs::exists(activePath / "ready"),
          "an active writer's locked workspace is never scavenged");
    if (activeLeaseFd >= 0) {
        ::flock(activeLeaseFd, LOCK_UN);
        ::close(activeLeaseFd);
    }

    monolith::detail::scavengeAtomicTempDirectories(parent);
    check(!fs::exists(activePath),
          "cleanup reclaims an unlocked interrupted workspace");

    constexpr unsigned long long unmarkedSequence = 20;
    const fs::path unmarkedPath = workspacePath(
        parent, target, unmarkedSequence,
        monolith::detail::atomicTempOlderPrefix);
    ec.clear();
    const bool unmarkedFixtureReady = fs::create_directory(unmarkedPath, ec) && !ec
        && writeFixture(unmarkedPath / "content", "possibly active");
    const bool unmarkedWrite = unmarkedFixtureReady
        && monolith::detail::writeTextAtomically(
            target, [](std::ostream& out) { out << "leave unmarked"; });
    check(unmarkedWrite && fs::exists(unmarkedPath / "content"),
          "an incomplete v2 workspace is left untouched for older active writers");

    const fs::path legacyPath = parent / ".monolith-tmp-123-4";
    ec.clear();
    const bool legacyFixtureReady = fs::create_directory(legacyPath, ec) && !ec
        && writeFixture(legacyPath / "content", "legacy workspace");
    const bool legacyWrite = legacyFixtureReady
        && monolith::detail::writeTextAtomically(
            target, [](std::ostream& out) { out << "preserve legacy"; });
    check(legacyWrite && fs::exists(legacyPath / "content"),
          "legacy workspaces without lease metadata are left untouched");

    const fs::path incompleteParent = parent / "incomplete";
    ec.clear();
    const bool incompleteParentReady = fs::create_directory(incompleteParent, ec) && !ec;
    const fs::path incompleteTarget = incompleteParent / "record.txt";
    const fs::path incompletePath = workspacePath(incompleteParent, incompleteTarget, 31);
    const fs::path previousStalePath = workspacePath(
        incompleteParent, incompleteTarget, 32,
        monolith::detail::atomicTempPreviousPrefix);
    const fs::path olderStalePath = workspacePath(
        incompleteParent, incompleteTarget, 33,
        monolith::detail::atomicTempOlderPrefix);
    const fs::path previousIncompletePath = workspacePath(
        incompleteParent, incompleteTarget, 34,
        monolith::detail::atomicTempPreviousPrefix);
    const bool incompleteFixturesReady = incompleteParentReady
        && fs::create_directory(incompletePath, ec) && !ec
        && writeOwnerMarker(incompletePath)
        && writeFixture(incompletePath / "lease", "")
        && fs::create_directory(previousStalePath, ec) && !ec
        && writeFixture(previousStalePath / "lease", "")
        && writeFixture(previousStalePath / "ready", "")
        && writeFixture(previousStalePath / "content", "old marked workspace");
    const bool olderFixturesReady = incompleteFixturesReady
        && fs::create_directory(olderStalePath, ec) && !ec
        && writeFixture(olderStalePath / "lease", "")
        && writeFixture(olderStalePath / "ready", "")
        && writeFixture(olderStalePath / "content", "older marked workspace")
        && fs::create_directory(previousIncompletePath, ec) && !ec
        && writeFixture(previousIncompletePath / "content", "old unmarked workspace");
    const bool incompleteRecoveryWrite = olderFixturesReady
        && monolith::detail::writeTextAtomically(
            incompleteTarget, [](std::ostream& out) { out << "recovered v3"; });
    check(incompleteRecoveryWrite && !fs::exists(incompletePath)
              && !fs::exists(previousStalePath) && !fs::exists(olderStalePath)
              && fs::exists(previousIncompletePath / "content"),
          "a first-touch sweep reclaims owned incomplete and marked legacy workspaces");

    const fs::path incompleteWithUserData = parent
        / (std::string(monolith::detail::atomicTempPrefix) + "incomplete-with-user-data");
    const fs::path readyWithUserData = parent
        / (std::string(monolith::detail::atomicTempPrefix) + "ready-with-user-data");
    ec.clear();
    const bool markedUserDataFixturesReady =
        fs::create_directory(incompleteWithUserData, ec) && !ec
        && writeOwnerMarker(incompleteWithUserData)
        && writeFixture(incompleteWithUserData / "lease", "")
        && writeFixture(incompleteWithUserData / "notes.txt", "keep incomplete notes")
        && fs::create_directory(readyWithUserData, ec) && !ec
        && writeOwnerMarker(readyWithUserData)
        && writeFixture(readyWithUserData / "lease", "")
        && writeFixture(readyWithUserData / "ready", "")
        && writeFixture(readyWithUserData / "content", "partial snapshot")
        && writeFixture(readyWithUserData / "notes.txt", "keep ready notes");
    if (markedUserDataFixturesReady) {
        monolith::detail::scavengeAtomicTempDirectories(parent);
    }
    check(markedUserDataFixturesReady
              && fs::exists(incompleteWithUserData / "notes.txt")
              && fs::exists(incompleteWithUserData / "owner")
              && fs::exists(incompleteWithUserData / "lease")
              && fs::exists(readyWithUserData / "notes.txt")
              && fs::exists(readyWithUserData / "owner")
              && fs::exists(readyWithUserData / "lease")
              && fs::exists(readyWithUserData / "ready")
              && fs::exists(readyWithUserData / "content"),
          "marked interrupted workspaces with unexpected entries are preserved");

    const fs::path activeUnexpectedParent = parent / "active-unexpected";
    ec.clear();
    const bool activeUnexpectedParentReady =
        fs::create_directory(activeUnexpectedParent, ec) && !ec;
    const fs::path activeUnexpectedTarget = activeUnexpectedParent / "record.txt";
    fs::path activeUnexpectedWorkspace;
    bool activeUnexpectedEntryCreated = false;
    const bool activeUnexpectedWrite = activeUnexpectedParentReady
        && monolith::detail::writeTextAtomically(
            activeUnexpectedTarget,
            [&](std::ostream& out) {
                for (const auto& candidate : fs::directory_iterator(activeUnexpectedParent)) {
                    if (candidate.path().filename().string().starts_with(
                            monolith::detail::atomicTempPrefix)) {
                        activeUnexpectedWorkspace = candidate.path();
                        break;
                    }
                }
                activeUnexpectedEntryCreated = !activeUnexpectedWorkspace.empty()
                    && writeFixture(activeUnexpectedWorkspace / "notes.txt", "keep active notes");
                out << "saved";
            });
    if (activeUnexpectedParentReady) {
        monolith::detail::scavengeAtomicTempDirectories(activeUnexpectedParent);
    }
    check(activeUnexpectedWrite, "atomic save succeeds with an extra workspace entry");
    check(activeUnexpectedEntryCreated,
          "write callback can place a user entry in the owned workspace");
    check(fs::exists(activeUnexpectedTarget), "atomic save publishes content with extra entry");
    check(!activeUnexpectedWorkspace.empty()
              && fs::exists(activeUnexpectedWorkspace / "notes.txt"),
          "active-save cleanup and later sweeps preserve unexpected entries");
    check(monolith::detail::hasAtomicTempOwnerMarker(activeUnexpectedWorkspace)
              && fs::exists(activeUnexpectedWorkspace / "lease")
              && fs::exists(activeUnexpectedWorkspace / "ready"),
          "preserved active workspace retains its ownership and recovery markers");

    const fs::path secondParent = parent / "second";
    ec.clear();
    const bool secondParentReady = fs::create_directory(secondParent, ec) && !ec;
    const fs::path secondTarget = secondParent / "document.txt";
    const fs::path secondStalePath = workspacePath(secondParent, secondTarget, 90);
    const bool secondStaleReady = secondParentReady
        && fs::create_directory(secondStalePath, ec) && !ec
        && writeOwnerMarker(secondStalePath)
        && writeFixture(secondStalePath / "lease", "")
        && writeFixture(secondStalePath / "ready", "")
        && writeFixture(secondStalePath / "content", "second-parent remnant");
    const bool secondParentWrite = secondStaleReady
        && monolith::detail::writeTextAtomically(
            secondTarget, [](std::ostream& out) { out << "fresh"; });
    check(secondParentWrite && !fs::exists(secondStalePath),
          "a newly seen destination parent is swept on its first save");

    constexpr unsigned long long intervalStaleSequence = 91;
    const fs::path intervalStalePath =
        workspacePath(secondParent, secondTarget, intervalStaleSequence);
    ec.clear();
    const bool intervalFixtureReady = fs::create_directory(intervalStalePath, ec) && !ec
        && writeOwnerMarker(intervalStalePath)
        && writeFixture(intervalStalePath / "lease", "")
        && writeFixture(intervalStalePath / "ready", "")
        && writeFixture(intervalStalePath / "content", "periodic remnant");
    bool intervalWritesSucceeded = intervalFixtureReady;
    for (unsigned write = 0; write < monolith::detail::atomicTempSweepInterval - 1;
         ++write) {
        intervalWritesSucceeded = intervalWritesSucceeded
            && monolith::detail::writeTextAtomically(
                secondTarget, [](std::ostream& out) { out << "steady state"; });
    }
    const bool retainedBeforeSweep = fs::exists(intervalStalePath / "content");
    const bool periodicSweepWrite = monolith::detail::writeTextAtomically(
        secondTarget, [](std::ostream& out) { out << "periodic sweep"; });
    check(intervalWritesSucceeded && retainedBeforeSweep && periodicSweepWrite
              && !fs::exists(intervalStalePath),
          "each destination parent is swept again after its write interval");

    bool rotationWritesSucceeded = true;
    for (std::size_t directory = 0;
         directory < monolith::detail::atomicTempTrackedParents;
         ++directory) {
        const fs::path rotatingParent = parent / ("rotation-" + std::to_string(directory));
        ec.clear();
        rotationWritesSucceeded = rotationWritesSucceeded
            && fs::create_directory(rotatingParent, ec) && !ec
            && monolith::detail::writeTextAtomically(
                rotatingParent / "record.txt",
                [](std::ostream& out) { out << "rotate"; });
    }
    constexpr unsigned long long evictedStaleSequence = 92;
    const fs::path evictedStalePath =
        workspacePath(secondParent, secondTarget, evictedStaleSequence);
    ec.clear();
    const bool evictedFixtureReady = fs::create_directory(evictedStalePath, ec) && !ec
        && writeOwnerMarker(evictedStalePath)
        && writeFixture(evictedStalePath / "lease", "")
        && writeFixture(evictedStalePath / "ready", "")
        && writeFixture(evictedStalePath / "content", "evicted-parent remnant");
    const bool evictedParentWrite = evictedFixtureReady
        && monolith::detail::writeTextAtomically(
            secondTarget, [](std::ostream& out) { out << "after eviction"; });
    check(rotationWritesSucceeded && evictedParentWrite && !fs::exists(evictedStalePath),
          "a destination parent is swept again after tracker eviction");

    if (failures == 0) {
        std::cout << "ALL ATOMIC FILE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
