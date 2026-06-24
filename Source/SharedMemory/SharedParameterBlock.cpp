#include "SharedParameterBlock.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <cstring>

size_t SharedParameterBlock::shmSize() noexcept { return sizeof(Layout); }

SharedParameterBlock::SharedParameterBlock()
{
    openMapping();
}

SharedParameterBlock::~SharedParameterBlock()
{
    closeMapping();
}

bool SharedParameterBlock::openMapping()
{
#if defined(_WIN32)
    mappingHandle_ = ::CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                          0, static_cast<DWORD>(shmSize()), "Local\\steinbach_console_v1");
    if (!mappingHandle_)
        return false;

    void* ptr = ::MapViewOfFile(static_cast<HANDLE>(mappingHandle_),
                                FILE_MAP_ALL_ACCESS, 0, 0, shmSize());
    if (!ptr)
    {
        ::CloseHandle(static_cast<HANDLE>(mappingHandle_));
        mappingHandle_ = nullptr;
        return false;
    }
#else
    // O_CREAT | O_RDWR: create if absent, otherwise open existing
    fd_ = ::shm_open(kShmName, O_CREAT | O_RDWR, 0600);
    if (fd_ < 0)
        return false;

    // Grow the region to full size if needed (safe to call even if already correct size)
    struct stat st;
    if (::fstat(fd_, &st) == 0 && static_cast<size_t>(st.st_size) < shmSize())
    {
        if (::ftruncate(fd_, static_cast<off_t>(shmSize())) != 0)
        {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
    }

    void* ptr = ::mmap(nullptr, shmSize(),
                       PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED)
    {
        ::close(fd_);
        fd_ = -1;
        return false;
    }
#endif

    data_ = static_cast<Layout*>(ptr);

#if defined(_WIN32)
    const uint32_t thisPid = static_cast<uint32_t>(::GetCurrentProcessId());
#else
    const uint32_t thisPid = static_cast<uint32_t>(::getpid());
#endif

    // First opener: magic CAS from 0 → kMagic initialises fresh memory.
    uint32_t expected = 0;
    if (data_->magic.compare_exchange_strong(expected, kMagic,
            std::memory_order_acq_rel, std::memory_order_acquire))
    {
        // Fresh memory (OS-zeroed). Store session PID and refCount.
        data_->sessionPid.store(thisPid, std::memory_order_relaxed);
        data_->refCount.store(1, std::memory_order_release);
    }
    else
    {
        // Memory already exists. Check if it's from a previous DAW session.
        uint32_t storedPid = data_->sessionPid.load(std::memory_order_acquire);
        if (storedPid != thisPid &&
            data_->sessionPid.compare_exchange_strong(storedPid, thisPid,
                std::memory_order_acq_rel, std::memory_order_relaxed))
        {
            // We won the race: wipe all stale channels from the previous session.
            for (int i = 0; i < kMaxChannels; ++i)
            {
                data_->channels[i].hasOverride.store(0, std::memory_order_relaxed);
                data_->channels[i].active.store(0, std::memory_order_release);
            }
            data_->refCount.store(1, std::memory_order_release);
        }
        else
        {
            data_->refCount.fetch_add(1, std::memory_order_acq_rel);
        }
    }
    return true;
}

void SharedParameterBlock::closeMapping()
{
    if (data_)
    {
        const uint32_t remaining = data_->refCount.fetch_sub(1,
            std::memory_order_acq_rel) - 1;

        // Last one out: unlink the shared memory region so it's recreated fresh next session.
#if ! defined(_WIN32)
        if (remaining == 0)
            ::shm_unlink(kShmName);
#endif

#if defined(_WIN32)
        ::UnmapViewOfFile(data_);
#else
        ::munmap(data_, shmSize());
#endif
        data_ = nullptr;
    }
#if defined(_WIN32)
    if (mappingHandle_)
    {
        ::CloseHandle(static_cast<HANDLE>(mappingHandle_));
        mappingHandle_ = nullptr;
    }
#else
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

int SharedParameterBlock::acquireChannel(const char* initialName) noexcept
{
    if (!data_) return -1;
    for (int i = 0; i < kMaxChannels; ++i)
    {
        uint32_t expected = 0;
        if (data_->channels[i].active.compare_exchange_strong(expected, 1u,
                std::memory_order_acq_rel, std::memory_order_relaxed))
        {
            // Reset override so stale data from a previous session doesn't linger
            data_->channels[i].hasOverride.store(0, std::memory_order_relaxed);
            writeName(i, initialName);
            return i;
        }
    }
    return -1;
}

void SharedParameterBlock::releaseChannel(int idx) noexcept
{
    if (!data_ || idx < 0 || idx >= kMaxChannels) return;
    data_->channels[idx].hasOverride.store(0, std::memory_order_relaxed);
    data_->channels[idx].active.store(0, std::memory_order_release);
}

SharedParameterBlock::Channel* SharedParameterBlock::get(int idx) noexcept
{
    if (!data_ || idx < 0 || idx >= kMaxChannels) return nullptr;
    return &data_->channels[idx];
}

const SharedParameterBlock::Channel* SharedParameterBlock::get(int idx) const noexcept
{
    if (!data_ || idx < 0 || idx >= kMaxChannels) return nullptr;
    return &data_->channels[idx];
}

void SharedParameterBlock::writeName(int idx, const char* utf8) noexcept
{
    if (!data_ || idx < 0 || idx >= kMaxChannels) return;
    auto& ch = data_->channels[idx];
    ch.nameSeq.fetch_add(1, std::memory_order_release); // odd = write in progress
    std::strncpy(ch.name, utf8, sizeof(ch.name) - 1);
    ch.name[sizeof(ch.name) - 1] = '\0';
    ch.nameSeq.fetch_add(1, std::memory_order_release); // even = done
}

bool SharedParameterBlock::readName(int idx, char* out, int outLen) const noexcept
{
    if (!data_ || idx < 0 || idx >= kMaxChannels) return false;
    const auto& ch = data_->channels[idx];
    const uint32_t seq1 = ch.nameSeq.load(std::memory_order_acquire);
    if (seq1 & 1u) return false; // write in progress
    std::strncpy(out, ch.name, static_cast<size_t>(outLen - 1));
    out[outLen - 1] = '\0';
    const uint32_t seq2 = ch.nameSeq.load(std::memory_order_acquire);
    return seq1 == seq2;
}

int SharedParameterBlock::activeCount() const noexcept
{
    if (!data_) return 0;
    int n = 0;
    for (int i = 0; i < kMaxChannels; ++i)
        if (data_->channels[i].active.load(std::memory_order_relaxed))
            ++n;
    return n;
}
