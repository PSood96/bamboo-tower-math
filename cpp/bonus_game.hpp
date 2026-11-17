#pragma once

/**
 * Bamboo Tower Cash-Fall Bonus (+ Coin Respin hooks).
 *
 * Proto rules (section stubs for later implementation):
 *
 * Coin Respin (base feature, not free spins):
 *   - Trigger: Collector lands with <6 held coins
 *   - Open cells use Respin pack (blank / coin / mult / collector)
 *   - 3 respins, reset on new coin or multiplier
 *   - Armed collectors sweep held total when respins end
 *   - Full 5×3 coin board → double collect (plus armed collectors)
 *
 * Cash-Fall Bonus:
 *   - Trigger: 6+ held coins at spin end (collector discarded if same spin)
 *   - Per-reel towers height TOWER_H = {3,4,5,6,7}; board grows to 7 rows
 *   - Held coins carry into same-reel tower slots (bottom-up)
 *   - Each live reel starts with 3 spins; reset on new coin / specials
 *   - Specials: COLL (collect tower or arm), REPL (copy), HEART (revive),
 *     MULT (boost a tower coin)
 *   - End: all reels dead → leftover tower coins forfeited (no closing collect)
 *
 * This header only scaffolds types + a no-op playCashFall for the sim loop.
 */

#include "config.hpp"
#include "rng.hpp"
#include "stats.hpp"
#include "types.hpp"

#include <array>
#include <vector>

namespace bambootower {

struct FeatureResult {
  double boardX = 0.0;  // × total bet from Cash-Fall
  int steps = 0;        // bonus steps / reel spins resolved
  int towersCashed = 0;
  bool winCapReached = false;
};

struct TowerState {
  std::array<std::vector<Coin>, COLS> towers{};
  std::array<int, COLS> spinsLeft{};
  std::array<bool, COLS> dead{};
  std::array<bool, COLS> pendingCollector{};
};

inline TowerState makeFreshTowers(const Config& cfg) {
  TowerState t;
  for (int c = 0; c < COLS; ++c) {
    t.spinsLeft[static_cast<size_t>(c)] = cfg.bonusReelSpins;
    t.dead[static_cast<size_t>(c)] = false;
    t.pendingCollector[static_cast<size_t>(c)] = false;
  }
  return t;
}

/** Capacity of reel `c` tower (proto TOWER_H). */
inline int towerCapacity(int c) {
  if (c < 0 || c >= COLS) return 0;
  return TOWER_H[static_cast<size_t>(c)];
}

/**
 * Play Cash-Fall under remaining ×bet budget — STUB.
 * Returns zero until the bonus step loop is implemented section-by-section.
 */
inline FeatureResult playCashFall(Rng& /*rng*/, const Config& /*cfg*/, Stats& /*st*/,
                                  double /*capX*/,
                                  const std::vector<Coin>& /*carryHeld*/ = {}) {
  FeatureResult out;
  // Future: seed towers from carryHeld, loop while any reel live, resolve
  // coin / COLL / REPL / HEART / MULT, forfeit leftovers.
  return out;
}

/**
 * Buy / force entry into Cash-Fall (sim mode cashfall) — STUB.
 * Mirrors wild-descent buy→FS jump once bonus rules land.
 */
inline FeatureResult playCashFallBuy(Rng& rng, const Config& cfg, Stats& st,
                                     double capX) {
  return playCashFall(rng, cfg, st, capX, {});
}

} // namespace bambootower
