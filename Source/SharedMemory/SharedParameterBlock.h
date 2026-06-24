#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

/**
 * POSIX-shared-memory parameter block.
 *
 * Enables cross-plugin parameter sharing between SteinbachChanelStrip instances
 * and the SteinbachConsole plugin within the same DAW session.
 *
 * Design:
 *  - Both plugins call shm_open("/steinbach_console_v1") to map the same memory.
 *  - ChannelStrip writes current param values every processBlock.
 *  - Console reads them for display; writes override values on user interaction.
 *  - ChannelStrip applies overrides in processBlock when hasOverride == 1.
 *
 * Thread safety: all Channel fields use lock-free atomics (arm64/x86_64).
 *
 * Note: std::atomic objects are placed in POSIX shared memory. This relies on
 * lock-free atomics mapping to plain load/store instructions with memory fences,
 * which is true on arm64 and x86_64 (static_assert below enforces this).
 */
class SharedParameterBlock
{
public:
    static constexpr int         kMaxChannels = 32;
    static constexpr const char* kShmName     = "/steinbach_console_v1";
    static constexpr uint32_t    kMagic       = 0x53424350u; // 'SBCP'

    // ── One channel strip's shared state ──────────────────────────────────────
    struct alignas(128) Channel
    {
        std::atomic<uint32_t> active      { 0 };   ///< 1 = slot in use

        // Current parameter values (written by ChannelStrip, read by Console)
        std::atomic<float>    preampDb    { 0.0f }; ///< -18..+18 dB
        std::atomic<float>    pan         { 0.0f }; ///< -1..+1
        std::atomic<float>    eqLow       { 0.0f }; ///< -12..+12 dB
        std::atomic<float>    eqMid       { 0.0f };
        std::atomic<float>    eqHigh      { 0.0f };
        std::atomic<float>    morph       { 0.0f }; ///< 0..1
        std::atomic<int32_t>  wavetable   { 0    }; ///< 0/1/2
        std::atomic<uint32_t> hpfEnabled  { 0    }; ///< 0/1

        // Console override (written by Console, applied by ChannelStrip in processBlock)
        std::atomic<uint32_t> hasOverride { 0    };
        std::atomic<float>    ovPreampDb  { 0.0f };
        std::atomic<float>    ovPan       { 0.0f };
        std::atomic<float>    ovEqLow     { 0.0f };
        std::atomic<float>    ovEqMid     { 0.0f };
        std::atomic<float>    ovEqHigh    { 0.0f };
        std::atomic<float>    ovMorph     { 0.0f };
        std::atomic<int32_t>  ovWavetable { 0    };
        std::atomic<uint32_t> ovHpfEnabled{ 0    };

        // Channel name (UTF-8, max 31 chars; seqlock for non-RT access)
        std::atomic<uint32_t> nameSeq     { 0 };
        char                  name[32]    {};
        char                  _pad[4]     {};
        // total = 128 bytes (aligned to 2 cache lines)
    };

    static_assert(sizeof(Channel) == 128,
        "SharedParameterBlock::Channel must be 128 bytes");
    static_assert(std::atomic<float>::is_always_lock_free,
        "SharedParameterBlock requires lock-free float atomics");
    static_assert(std::atomic<uint32_t>::is_always_lock_free,
        "SharedParameterBlock requires lock-free uint32 atomics");

    SharedParameterBlock();
    ~SharedParameterBlock();

    bool isValid() const noexcept { return data_ != nullptr; }

    /** Acquire a channel slot (non-RT, call once at construction). Returns idx or -1. */
    int  acquireChannel(const char* initialName = "CH") noexcept;
    /** Release a channel slot (non-RT, call at destruction). */
    void releaseChannel(int idx) noexcept;

    Channel*       get(int idx) noexcept;
    const Channel* get(int idx) const noexcept;

    /** Write channel name (non-RT, seqlock write). */
    void writeName(int idx, const char* utf8) noexcept;
    /** Read channel name into out buffer (non-RT). Returns false if write was in progress. */
    bool readName(int idx, char* out, int outLen) const noexcept;

    /** Count of currently active channels. */
    /** Total size of the shared memory region. */
    static size_t shmSize() noexcept;

    int activeCount() const noexcept;

    /** Call fn(idx, const Channel&) for every active channel. */
    template<typename Fn>
    void forEachActive(Fn&& fn) const noexcept
    {
        if (!data_) return;
        for (int i = 0; i < kMaxChannels; ++i)
            if (data_->channels[i].active.load(std::memory_order_acquire))
                fn(i, data_->channels[i]);
    }

private:
    struct Layout
    {
        std::atomic<uint32_t> magic      { 0 };
        std::atomic<uint32_t> refCount   { 0 };
        std::atomic<uint32_t> sessionPid { 0 };  ///< PID of owning process; reset channels if changed
        char                  reserved[52];
        Channel               channels[kMaxChannels];
    };

    int     fd_   { -1      };
    Layout* data_ { nullptr };

    bool openMapping();
    void closeMapping();
};
