#include "AtomicFile.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include <unistd.h>

namespace monolith::detail {

AtomicTempOutputBuffer::AtomicTempOutputBuffer(int fd) : fd_(fd) {
    setp(buffer_.data(), buffer_.data() + buffer_.size());
}

AtomicTempOutputBuffer::~AtomicTempOutputBuffer() {
    if (fd_ >= 0) ::close(fd_);
}

bool AtomicTempOutputBuffer::flushBuffer() {
    if (failed_ || fd_ < 0) return false;
    const std::size_t size = static_cast<std::size_t>(pptr() - pbase());
    std::size_t written = 0;
    while (written < size) {
        const ssize_t result = ::write(fd_, pbase() + written, size - written);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) {
            failed_ = true;
            setp(buffer_.data(), buffer_.data() + buffer_.size());
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    setp(buffer_.data(), buffer_.data() + buffer_.size());
    return true;
}

AtomicTempOutputBuffer::int_type AtomicTempOutputBuffer::overflow(
    int_type character) {
    if (failed_ || fd_ < 0 || !flushBuffer()) return traits_type::eof();
    if (!traits_type::eq_int_type(character, traits_type::eof())) {
        *pptr() = traits_type::to_char_type(character);
        pbump(1);
    }
    return traits_type::not_eof(character);
}

std::streamsize AtomicTempOutputBuffer::xsputn(const char* data,
                                               std::streamsize size) {
    if (size <= 0 || failed_ || fd_ < 0) return 0;
    std::streamsize written = 0;
    while (written < size) {
        if (pptr() == epptr() && !flushBuffer()) break;
        const auto available = static_cast<std::streamsize>(epptr() - pptr());
        const auto chunk = std::min(available, size - written);
        std::memcpy(pptr(), data + written, static_cast<std::size_t>(chunk));
        pbump(static_cast<int>(chunk));
        written += chunk;
    }
    return written;
}

int AtomicTempOutputBuffer::sync() {
    return flushBuffer() ? 0 : -1;
}

AtomicTempOutputBuffer::pos_type AtomicTempOutputBuffer::seekoff(
    off_type offset,
    std::ios_base::seekdir direction,
    std::ios_base::openmode which) {
    if ((which & std::ios_base::out) == 0 || fd_ < 0 || sync() != 0) {
        return pos_type(off_type(-1));
    }

    int whence;
    if (direction == std::ios_base::beg) {
        whence = SEEK_SET;
    } else if (direction == std::ios_base::cur) {
        whence = SEEK_CUR;
    } else if (direction == std::ios_base::end) {
        whence = SEEK_END;
    } else {
        return pos_type(off_type(-1));
    }

    const off_t nativeOffset = static_cast<off_t>(offset);
    if (static_cast<off_type>(nativeOffset) != offset) {
        return pos_type(off_type(-1));
    }
    const off_t position = ::lseek(fd_, nativeOffset, whence);
    if (position < 0) return pos_type(off_type(-1));
    return pos_type(static_cast<off_type>(position));
}

AtomicTempOutputBuffer::pos_type AtomicTempOutputBuffer::seekpos(
    pos_type position,
    std::ios_base::openmode which) {
    return seekoff(static_cast<off_type>(position), std::ios_base::beg, which);
}

bool AtomicTempOutputBuffer::close() {
    const bool flushed = sync() == 0;
    if (fd_ < 0) return flushed;
    const int fd = std::exchange(fd_, -1);
    return ::close(fd) == 0 && flushed;
}

} // namespace monolith::detail
