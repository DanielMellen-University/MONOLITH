#include "../src/detail/AtomicFile.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/file.h>
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

    const fs::path parent = fs::temp_directory_path()
        / ("monolith-atomic-file-" + std::to_string(getpid()));
    std::error_code ec;
    fs::remove_all(parent, ec);
    fs::create_directories(parent, ec);
    if (ec) {
        std::cerr << "FAIL: could not create atomic-file test directory\n";
        return 1;
    }

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

    const fs::path markerParent = parent / "marker-check";
    ec.clear();
    const bool markerParentReady = fs::create_directory(markerParent, ec) && !ec;
    bool markerObservedDuringSave = false;
    const bool markerWrite = markerParentReady
        && monolith::detail::writeTextAtomically(
            markerParent / "record.txt",
            [&](std::ostream& out) {
                for (const auto& entry : fs::directory_iterator(markerParent)) {
                    if (!entry.path().filename().string().starts_with(
                            monolith::detail::atomicTempPrefix)) {
                        continue;
                    }
                    markerObservedDuringSave = monolith::detail::hasAtomicTempOwnerMarker(
                        entry.path())
                        && fs::is_regular_file(entry.path() / "ready")
                        && fs::is_regular_file(entry.path() / "lease");
                    break;
                }
                out << "owned";
            });
    check(markerWrite && markerObservedDuringSave,
          "new v4 workspaces carry a valid owner marker before writing content");

    const fs::path target = parent / "settings.txt";
    const auto stalePath = workspacePath(parent, target, 7);
    fs::create_directory(stalePath, ec);
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
    monolith::detail::atomicTempSequence.store(7, std::memory_order_relaxed);
    const bool staleWrite = staleFixturesReady && userIncompleteReady && userMarkedReady
        && monolith::detail::writeTextAtomically(
            target, [](std::ostream& out) { out << "recovered"; });
    check(staleWrite && !fs::exists(stalePath)
              && fs::exists(target) && fs::file_size(target, ec) == 9,
          "a marked unlocked workspace is reclaimed after an interrupted save");
    check(userIncompleteReady && userMarkedReady
              && fs::exists(userIncompletePath / "notes.txt")
              && fs::exists(userMarkedPath / "notes.txt"),
          "lookalike directories without a valid ownership marker survive cleanup");

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
    monolith::detail::atomicTempSequence.store(activeSequence, std::memory_order_relaxed);
    const bool activeWrite = activeFixtureReady && activeLockHeld
        && monolith::detail::writeTextAtomically(
            target, [](std::ostream& out) { out << "concurrent writer"; });
    std::string activeContent;
    std::ifstream activeInput(activePath / "content", std::ios::binary);
    std::getline(activeInput, activeContent);
    check(activeWrite && activeContent == "active partial snapshot"
              && fs::exists(activePath / "ready"),
          "an active writer's locked workspace is never scavenged");
    if (activeLeaseFd >= 0) {
        ::flock(activeLeaseFd, LOCK_UN);
        ::close(activeLeaseFd);
    }

    monolith::detail::atomicTempSequence.store(activeSequence, std::memory_order_relaxed);
    const bool retryWrite = monolith::detail::writeTextAtomically(
        target, [](std::ostream& out) { out << "after recovery"; });
    check(retryWrite && !fs::exists(activePath),
          "a later write reclaims an unlocked interrupted candidate");

    constexpr unsigned long long unmarkedSequence = 20;
    const fs::path unmarkedPath = workspacePath(
        parent, target, unmarkedSequence,
        monolith::detail::atomicTempOlderPrefix);
    ec.clear();
    const bool unmarkedFixtureReady = fs::create_directory(unmarkedPath, ec) && !ec
        && writeFixture(unmarkedPath / "content", "possibly active");
    monolith::detail::atomicTempSequence.store(unmarkedSequence, std::memory_order_relaxed);
    const bool unmarkedWrite = unmarkedFixtureReady
        && monolith::detail::writeTextAtomically(
            target, [](std::ostream& out) { out << "leave unmarked"; });
    check(unmarkedWrite && fs::exists(unmarkedPath / "content"),
          "an incomplete v2 workspace is left untouched for older active writers");

    const fs::path legacyPath = parent / ".monolith-tmp-123-4";
    ec.clear();
    const bool legacyFixtureReady = fs::create_directory(legacyPath, ec) && !ec
        && writeFixture(legacyPath / "content", "legacy workspace");
    monolith::detail::atomicTempSequence.store(40, std::memory_order_relaxed);
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
        && writeFixture(incompletePath / "content", "crashed during setup")
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
    monolith::detail::atomicTempSequence.store(0, std::memory_order_relaxed);
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
    monolith::detail::atomicTempSequence.store(0, std::memory_order_relaxed);
    const bool evictedParentWrite = evictedFixtureReady
        && monolith::detail::writeTextAtomically(
            secondTarget, [](std::ostream& out) { out << "after eviction"; });
    check(rotationWritesSucceeded && evictedParentWrite && !fs::exists(evictedStalePath),
          "a destination parent is swept again after tracker eviction");

    fs::remove_all(parent, ec);
    if (failures == 0) {
        std::cout << "ALL ATOMIC FILE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
