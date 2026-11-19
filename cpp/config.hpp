#pragma once

#include "rng.hpp"
#include "types.hpp"

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace bambootower {

/**
 * Config / weight-pack scaffold (wild-descent shape).
 *
 * Reelset *definitions* are shared; per-mode packs pick among them.
 * Blank foundation: empty pick weights and empty column tables so structure
 * checks run before math is authored. Do not retune weights unless asked.
 *
 * Design hooks (from cashfall proto, not yet wired as live weight packs):
 *   Main   — pay symbols + wild + coin + collector
 *   Respin — blank / coin / mult / collector
 *   CashFall — tower fill + COLL / REPL / HEART / MULT specials (often procedural)
 */

enum class ReelDraw {
  H1, H2, H3, L1, L2, L3, L4, L5,
  WW, CN, CL, RP, MU, HT, BL,
  COUNT
};
constexpr int kReelDrawCount = static_cast<int>(ReelDraw::COUNT);

struct Reelset {
  std::string id;
  /**
   * Per-symbol, per-column weights: w[draw][col].
   * Zero everywhere until packs are authored (preference: no weight edits).
   */
  std::array<std::array<double, COLS>, kReelDrawCount> w{};
};

struct ModeWeightPack {
  std::vector<std::pair<std::string, double>> reelsetW;
  /**
   * Optional Cash-Fall procedural chances (scaffold zeros).
   * Proto used hand-tuned roll thresholds — leave unused until authored.
   */
  double pCoinLand = 0.0;
  double pCollector = 0.0;
  double pReplicator = 0.0;
  double pHeart = 0.0;
  double pMult = 0.0;
};

inline ModeWeightPack weightPackForMode(SimMode mode) {
  ModeWeightPack p;
  // Intentionally empty pick weights until packs are authored.
  (void)mode;
  return p;
}

inline Reelset makeEmptyReelset(const std::string& id) {
  Reelset rs;
  rs.id = id;
  // w default-initialized to 0
  return rs;
}

/**
 * Registered reelsets (shared across modes). Empty columns for structure only:
 *   Main1   — base game
 *   Respin1 — coin-respin open cells
 *   Bonus1  — Cash-Fall scaffold (may stay procedural)
 */
inline const std::vector<Reelset>& REELSETS() {
  static const std::vector<Reelset> sets{
    makeEmptyReelset("Main1"),
    makeEmptyReelset("Respin1"),
    makeEmptyReelset("Bonus1"),
  };
  return sets;
}

inline const Reelset& reelsetById(const std::string& id) {
  for (const auto& rs : REELSETS())
    if (rs.id == id) return rs;
  return REELSETS().front();
}

inline SymId reelDrawToSym(ReelDraw d) {
  switch (d) {
    case ReelDraw::H1: return SymId::H1;
    case ReelDraw::H2: return SymId::H2;
    case ReelDraw::H3: return SymId::H3;
    case ReelDraw::L1: return SymId::L1;
    case ReelDraw::L2: return SymId::L2;
    case ReelDraw::L3: return SymId::L3;
    case ReelDraw::L4: return SymId::L4;
    case ReelDraw::L5: return SymId::L5;
    case ReelDraw::WW: return SymId::W;
    case ReelDraw::CN: return SymId::CN;
    case ReelDraw::CL: return SymId::CL;
    case ReelDraw::RP: return SymId::RP;
    case ReelDraw::MU: return SymId::MU;
    case ReelDraw::HT: return SymId::HT;
    case ReelDraw::BL: return SymId::BL;
    default: return SymId::EM;
  }
}

inline const char* reelDrawName(ReelDraw d) {
  return symName(reelDrawToSym(d));
}

/**
 * Scaffold pay+wild fill pool (proto BASE_POOL ratios, features stripped).
 * NOT stored in REELSETS() — preference: do not edit weight tables.
 * Used when the active reelset column has no positive normal weights so
 * main-mode sims can still produce line wins before packs are authored.
 */
inline SymId pickScaffoldNormal(Rng& rng) {
  static const std::vector<std::pair<ReelDraw, double>> items = {
    {ReelDraw::H1, 32.0},  // FIRE7
    {ReelDraw::H2, 63.0},  // BELL
    {ReelDraw::H3, 63.0},  // CHR
    {ReelDraw::L1, 95.0},  // A
    {ReelDraw::L2, 127.0}, // K
    {ReelDraw::L3, 127.0}, // Q
    {ReelDraw::L4, 158.0}, // J
    {ReelDraw::L5, 158.0}, // 10
    {ReelDraw::WW, 63.0},  // WILD
  };
  return reelDrawToSym(rng.pickWeighted(items));
}

/**
 * Weighted draw from one reel column.
 * normalsOnly + empty column → scaffold pay/wild pool (not EM).
 * Full draw + empty column → EM (no invented feature lands from strips).
 */
inline SymId pickFromReel(Rng& rng, const Reelset& rs, int col,
                          bool normalsOnly = false) {
  if (col < 0) col = 0;
  if (col >= COLS) col = COLS - 1;
  std::vector<std::pair<ReelDraw, double>> items;
  items.reserve(static_cast<size_t>(kReelDrawCount));
  for (int i = 0; i < kReelDrawCount; ++i) {
    const auto d = static_cast<ReelDraw>(i);
    if (normalsOnly) {
      // Base fill step: pays + wild only (CN/CL placed procedurally).
      if (!(d <= ReelDraw::L5 || d == ReelDraw::WW)) continue;
    }
    double w = rs.w[static_cast<size_t>(i)][static_cast<size_t>(col)];
    if (w > 0.0) items.emplace_back(d, w);
  }
  if (items.empty()) {
    if (normalsOnly) return pickScaffoldNormal(rng);
    return SymId::EM;
  }
  return reelDrawToSym(rng.pickWeighted(items));
}

/** Pay + wild draw for remaining-cell fill after collector/coins. */
inline SymId pickNormalFromReel(Rng& rng, const Reelset& rs, int col) {
  return pickFromReel(rng, rs, col, /*normalsOnly=*/true);
}

inline const Reelset& pickReelset(Rng& rng, const ModeWeightPack& pack) {
  if (pack.reelsetW.empty()) return REELSETS().front();
  const std::string id = rng.pickWeighted(pack.reelsetW);
  return reelsetById(id);
}

/**
 * Proto COIN_VALUES / MULT_VALUES (placeholder, not tuned).
 * Uniform pick among listed entries — not reel weight tables.
 */
inline const std::vector<double>& COIN_VALUES() {
  static const std::vector<double> v = {
    0.25, 0.25, 0.25, 0.5, 0.5, 0.5, 1.0, 1.0, 1.0, 2.0, 2.0, 5.0
  };
  return v;
}

inline const std::vector<int>& MULT_VALUES() {
  static const std::vector<int> v = {2, 2, 2, 2, 3, 3, 3, 4, 5, 8};
  return v;
}

inline double pickCoinValue(Rng& rng) {
  return rng.pick(COIN_VALUES());
}

/**
 * Base board-gen cash value (user placeholder): uniform integer in [min, max].
 * Separate from proto COIN_VALUES table — not a reel weight table.
 */
inline int pickBoardCoinCashValue(Rng& rng, int minV, int maxV) {
  if (maxV < minV) {
    const int t = minV;
    minV = maxV;
    maxV = t;
  }
  if (minV == maxV) return minV;
  return minV + rng.nextInt(maxV - minV + 1);
}

inline int pickMultValue(Rng& rng) {
  return rng.pick(MULT_VALUES());
}

/** Default coin-count weights for counts 0..6 — all mass on 0 until authored. */
inline std::array<double, 7> defaultCoinCountW() {
  return {{1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
}

/**
 * Pick coin land count in 0..6 from config weights.
 * Empty / all-zero → 0 (safe placeholder).
 */
inline int pickCoinCount(Rng& rng, const std::array<double, 7>& weights) {
  std::vector<double> w(weights.begin(), weights.end());
  const int idx = rng.weightedIndex(w);
  if (idx < 0) return 0;
  return idx; // index == count
}

/**
 * Line pays as × total bet for oak 5 / 4 / 3 (matches proto PAY table shape).
 * Proto: FIRE7/BELL/CHR/A/K/Q/J/10 — mapped to H1..L5.
 * Placeholder values from the HTML — not a weight-table retune.
 */
inline double paytableTotalBet(SymId sym, int oak) {
  // Proto: runLen>=5 uses the 5oak row (board is only 5 wide, but clamp anyway).
  if (!isPaySym(sym) || oak < 3) return 0.0;
  auto row = [&](double a5, double a4, double a3) -> double {
    if (oak >= 5) return a5;
    if (oak == 4) return a4;
    return a3;
  };
  switch (sym) {
    case SymId::H1: return row(10.0, 5.0, 3.0); // FIRE7
    case SymId::H2: return row(8.0, 4.0, 2.0); // BELL
    case SymId::H3: return row(6.0, 3.0, 1.5); // CHR
    case SymId::L1: return row(3.0, 2.0, 1.0); // A
    case SymId::L2: return row(3.0, 2.0, 1.0); // K
    case SymId::L3: return row(2.0, 1.0, 0.5); // Q
    case SymId::L4: return row(2.0, 1.0, 0.5); // J
    case SymId::L5: return row(2.0, 1.0, 0.5); // 10
    default: return 0.0;
  }
}

/** Alias kept for harness / wild-descent naming familiarity. */
inline double paytableLineBet(SymId sym, int oak) {
  // Proto pays are already × total bet (not line-bet multiples).
  return paytableTotalBet(sym, oak);
}

/**
 * 30 fixed paylines — row index per reel (0=top). From proto LINES[].
 */
inline const std::vector<std::array<int, COLS>>& PAYLINES() {
  static const std::vector<std::array<int, COLS>> lines = {
    {{0,0,0,0,0}}, {{1,1,1,1,1}}, {{2,2,2,2,2}}, {{0,0,1,0,0}}, {{0,1,0,1,0}},
    {{0,1,1,1,0}}, {{0,1,2,1,0}}, {{1,0,0,0,1}}, {{1,0,1,0,1}}, {{1,1,0,1,1}},
    {{1,1,2,1,1}}, {{1,2,1,2,1}}, {{1,2,2,2,1}}, {{2,1,0,1,2}}, {{2,1,1,1,2}},
    {{2,1,2,1,2}}, {{2,2,1,2,2}}, {{0,0,0,0,1}}, {{0,0,0,1,0}}, {{0,0,0,1,1}},
    {{0,0,0,1,2}}, {{0,0,1,0,1}}, {{0,0,1,1,0}}, {{0,0,1,1,1}}, {{0,0,1,1,2}},
    {{0,0,1,2,1}}, {{0,0,1,2,2}}, {{0,1,0,0,0}}, {{0,1,0,0,1}}, {{0,1,0,1,1}},
  };
  return lines;
}

/** Every distinct landable token currently present on reelsets (weight > 0). */
inline std::vector<std::string> landableReelTokens() {
  std::vector<std::string> out;
  for (const auto& rs : REELSETS()) {
    for (int di = 0; di < kReelDrawCount; ++di) {
      for (int c = 0; c < COLS; ++c) {
        const double w = rs.w[static_cast<size_t>(di)][static_cast<size_t>(c)];
        if (!(w > 0.0) || !std::isfinite(w)) continue;
        const std::string tok = reelDrawName(static_cast<ReelDraw>(di));
        bool seen = false;
        for (const auto& t : out)
          if (t == tok) { seen = true; break; }
        if (!seen) out.push_back(tok);
      }
    }
  }
  return out;
}

/** Tokens on strips/weight tables that are outside the symbol list. */
inline std::vector<std::string> oddReelTokens() {
  std::vector<std::string> odd;
  for (const auto& tok : landableReelTokens()) {
    SymId id{};
    if (!parseSymName(tok, id) || !isStripToken(id))
      odd.push_back(tok);
  }
  return odd;
}

struct Config {
  double bet = BET;
  double maxWinX = MAX_WIN_X;
  int heldBonusTrigger = HELD_BONUS_TRIGGER;
  int baseRespinStart = BASE_RESPIN_START;
  int bonusReelSpins = BONUS_REEL_SPINS;
  int maxMultHits = MAX_MULT_HITS;
  ModeWeightPack pack{};

  /**
   * Base board-gen knobs (procedural — NOT reel weight tables).
   * Defaults leave collector/coins off so empty reel packs stay RTP-0 until
   * knobs / packs are authored. Override in tests or when tuning.
   */
  double pCollectorLand = 0.0;                 // P(place one collector)
  std::array<double, 7> coinCountW = defaultCoinCountW(); // weights for 0..6
  int coinValueMin = 1;                        // inclusive cash value
  int coinValueMax = 100;

  static Config defaults() { return forMode(SimMode::Main); }

  static Config forMode(SimMode mode) {
    Config c;
    c.pack = weightPackForMode(mode);
    // ModeWeightPack.pCollector is reserved for later pack wiring; board gen
    // uses Config::pCollectorLand so we do not invent reel weights.
    (void)c.pack.pCollector;
    return c;
  }
};

inline Config withWeightPack(Config cfg, SimMode mode) {
  cfg.pack = weightPackForMode(mode);
  return cfg;
}

} // namespace bambootower
