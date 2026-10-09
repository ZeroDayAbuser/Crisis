#pragma once

// ── Priority gating framework ───────────────────────────────────────────────
//
// Reduces IPC round-trips by skipping reads that can tolerate stale data.
// Each consumer owns a `priority::c_gate` and calls `gate.tick()` once per
// iteration, then guards reads with `if ( gate.on( priority::medium ) )`.
//
// Skipped reads retain their previously-cached member values — the renderer
// never sees uninitialised data, just slightly older data.
//
//   HIGH   — every tick          (aimbot-critical: bones, position, dying)
//   MEDIUM — every 4th tick      (weapon, ammo, flags that change occasionally)
//   LOW    — every 16th tick     (cosmetic: chams meshes, building state)
//
// In normal mode every level is promoted by one tier (LOW→MEDIUM, MEDIUM→HIGH)
// for snappier updates.  Performance mode uses the base rates above.
//
// The INFO tier (player names, rank, platform) is already handled by the
// existing 500 ms `refresh_info` timer and is not part of this system.

namespace priority {

    enum class level : std::uint32_t {
        high   = 1,
        medium = 4,
        low    = 16,
    };

    // Promote a level by one tier: low→medium, medium→high, high stays high.
    [[nodiscard]] inline level promote( level p ) {
        switch ( p ) {
            case level::low:    return level::medium;
            case level::medium: return level::high;
            default:            return level::high;
        }
    }

    class c_gate {
    public:
        // Call once at the top of each update iteration.
        void tick( ) { ++m_tick; }

        // Returns true when this tick should execute reads at the given priority.
        // In performance mode the base rates are used (HIGH=1, MEDIUM=4, LOW=16).
        // In normal mode every level is promoted by one tier for faster updates.
        //
        // LOW is phase-shifted by 2 so it never coincides with a MEDIUM tick,
        // spreading IPC load evenly and preventing ops/s spikes.
        [[nodiscard]] bool on( level p ) const {
            const auto effective = config::settings::performance_mode
                ? p : promote( p );
            const auto divisor = static_cast< std::uint32_t >( effective );
            if ( divisor <= 1 ) return true;
            const auto phase = ( divisor >= 16 ) ? 2u : 0u;
            return ( m_tick % divisor ) == phase;
        }

        [[nodiscard]] std::uint32_t count( ) const { return m_tick; }

    private:
        std::uint32_t m_tick { 0 };
    };

} // namespace priority
