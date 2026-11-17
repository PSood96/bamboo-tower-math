#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace bambootower {

/**
 * Bamboo Tower / Coin Collector · Cash-Fall Towers
 * Design reference: proto/coin-collector-cashfall-proto7-visual.html
 *
 * Base board: 5×3 · 30 fixed paylines.
 * Bonus towers: per-reel heights {3,4,5,6,7} → max rows 7.
 */

constexpr int COLS = 5;
constexpr int ROWS = 3;           // base game visible rows
constexpr int BONUS_ROWS = 7;     // tallest tower (reel 5)
constexpr int LINE_COUNT = 30;
constexpr double BET = 1.0;
constexpr double MAX_WIN_X = 10000.0;

/** Per-reel Cash-Fall tower capacities (proto TOWER_H). */
constexpr std::array<int, COLS> TOWER_H = {{3, 4, 5, 6, 7}};

constexpr int HELD_BONUS_TRIGGER = 6; // 6+ held coins → Cash-Fall Bonus
constexpr int BASE_RESPIN_START = 3;  // Coin Respin counter
constexpr int BONUS_REEL_SPINS = 3;   // per-reel Cash-Fall spins
constexpr int MAX_MULT_HITS = 3;      // per-coin multiplier boost cap

/**
 * Symbol list (math ids). Proto labels in comments.
 * Strip / landable tokens must stay inside this set ∪ engine specials.
 */
enum class SymId {
  H1,  // FIRE7
  H2,  // BELL
  H3,  // CHR (cherry)
  L1,  // A
  L2,  // K
  L3,  // Q
  L4,  // J
  L5,  // 10
  W,   // WILD
  CN,  // COIN
  CL,  // COLL (collector)
  RP,  // REPL (replicator) — bonus special / future strip
  MU,  // MULT (multiplier) — respin / bonus
  HT,  // HEART — bonus revive special
  BL,  // BLANK — respin open-cell fill
  EM,  // empty / engine-only (not a strip token)
  COUNT
};

inline bool isPaySym(SymId id) {
  return id >= SymId::H1 && id <= SymId::L5;
}

inline bool isHigh(SymId id) {
  return id >= SymId::H1 && id <= SymId::H3;
}

inline bool isLow(SymId id) {
  return id >= SymId::L1 && id <= SymId::L5;
}

inline bool isWild(SymId id) { return id == SymId::W; }
inline bool isCoin(SymId id) { return id == SymId::CN; }
inline bool isCollector(SymId id) { return id == SymId::CL; }
inline bool isReplicator(SymId id) { return id == SymId::RP; }
inline bool isMult(SymId id) { return id == SymId::MU; }
inline bool isHeart(SymId id) { return id == SymId::HT; }
inline bool isBlank(SymId id) { return id == SymId::BL; }

/** Feature symbols break line runs (proto evaluateLines). */
inline bool isFeatureSym(SymId id) {
  return isCoin(id) || isCollector(id) || isReplicator(id) || isMult(id) ||
         isHeart(id) || isBlank(id);
}

/** Tokens allowed on strips / landable weight tables (not EM). */
inline bool isStripToken(SymId id) {
  return isPaySym(id) || isWild(id) || isCoin(id) || isCollector(id) ||
         isReplicator(id) || isMult(id) || isHeart(id) || isBlank(id);
}

struct Cell {
  SymId sym = SymId::EM;
  double coinValue = 0.0; // × bet when CN
  int multHits = 0;       // multiplier boosts applied to this coin
  int rolledMult = 0;     // display value when MU lands
  bool held = false;      // locked coin / armed collector cell
};

/** Base board [col][row], row 0 = top. */
using Board = std::array<std::array<Cell, ROWS>, COLS>;

/** Held / tower coin object (proto {value, hits}). */
struct Coin {
  double value = 0.0;
  int hits = 0;
};

struct LineWin {
  int payline = 0; // 1-based
  SymId symbol = SymId::L5;
  int oak = 0;
  double payX = 0.0; // × total bet (proto pays payMult * bet)
};

struct RoundResult {
  double lineWin = 0.0;    // credits
  double collectWin = 0.0; // base collector / full-board collect credits
  double bonusWin = 0.0;   // Cash-Fall credits
  double total = 0.0;
  double totalX = 0.0;
  double featureX = 0.0;
  int heldAtEnd = 0;
  bool coinRespin = false;
  bool cashFall = false;
  bool winCapReached = false;
  Board board{};
};

/**
 * Sim / weight-pack modes.
 * Main = ordinary base spin pack.
 * Respin = coin-range pack (blank / coin / mult / collector).
 * CashFall = Cash-Fall Bonus procedural / pack scaffold.
 */
enum class SimMode { Main, Respin, CashFall };

inline const char* modeName(SimMode m) {
  switch (m) {
    case SimMode::Respin: return "respin";
    case SimMode::CashFall: return "cashfall";
    case SimMode::Main:
    default: return "main";
  }
}

inline double round2(double n) {
  return std::round(n * 100.0) / 100.0;
}

constexpr int kPaySymCount = 8; // H1..L5
constexpr int kOakCount = 3;    // 3,4,5
constexpr int kWinRangeCount = 20;
constexpr int kBaseWinRangeCount = 7;

inline const char* paySymName(int i) {
  static const char* names[kPaySymCount] = {
    "H1", "H2", "H3", "L1", "L2", "L3", "L4", "L5"
  };
  if (i < 0 || i >= kPaySymCount) return "?";
  return names[i];
}

inline int paySymIndex(SymId id) {
  int i = static_cast<int>(id);
  if (i < 0 || i >= kPaySymCount) return -1;
  return i;
}

inline const char* symName(SymId id) {
  static const char* names[] = {
    "H1", "H2", "H3", "L1", "L2", "L3", "L4", "L5",
    "WW", "CN", "CL", "RP", "MU", "HT", "BL", "EM"
  };
  const int i = static_cast<int>(id);
  if (i < 0 || i >= static_cast<int>(SymId::COUNT)) return "?";
  return names[i];
}

/** Parse strip spellings (math ids + common proto aliases). */
inline bool parseSymName(const std::string& s, SymId& out) {
  if (s == "H1" || s == "FIRE7") { out = SymId::H1; return true; }
  if (s == "H2" || s == "BELL") { out = SymId::H2; return true; }
  if (s == "H3" || s == "CHR") { out = SymId::H3; return true; }
  if (s == "L1" || s == "A") { out = SymId::L1; return true; }
  if (s == "L2" || s == "K") { out = SymId::L2; return true; }
  if (s == "L3" || s == "Q") { out = SymId::L3; return true; }
  if (s == "L4" || s == "J") { out = SymId::L4; return true; }
  if (s == "L5" || s == "10") { out = SymId::L5; return true; }
  if (s == "WW" || s == "W" || s == "WILD") { out = SymId::W; return true; }
  if (s == "CN" || s == "COIN") { out = SymId::CN; return true; }
  if (s == "CL" || s == "COLL") { out = SymId::CL; return true; }
  if (s == "RP" || s == "REPL") { out = SymId::RP; return true; }
  if (s == "MU" || s == "MULT") { out = SymId::MU; return true; }
  if (s == "HT" || s == "HEART") { out = SymId::HT; return true; }
  if (s == "BL" || s == "BLANK") { out = SymId::BL; return true; }
  return false;
}

inline const char* winRangeName(int i) {
  static const char* names[kWinRangeCount] = {
    "Win = (0)bet",
    "Win = (0-1]bet",
    "Win = (1-2]bet",
    "Win = (2-3]bet",
    "Win = (3-5]bet",
    "Win = (5-10]bet",
    "Win = (10-20]bet",
    "Win = (20-30]bet",
    "Win = (30-40]bet",
    "Win = (40-50]bet",
    "Win = (50-75]bet",
    "Win = (75-100]bet",
    "Win = (100-150]bet",
    "Win = (150-300]bet",
    "Win = (300-1000]bet",
    "Win = (1000-2000]bet",
    "Win = (2000-5000]bet",
    "Win = (5000-10000]bet",
    "Win = (10000-15000]bet",
    "Win = (15000-)bet"
  };
  if (i < 0 || i >= kWinRangeCount) return "?";
  return names[i];
}

inline int winRangeIndex(double x) {
  if (x <= 0.0) return 0;
  if (x <= 1.0) return 1;
  if (x <= 2.0) return 2;
  if (x <= 3.0) return 3;
  if (x <= 5.0) return 4;
  if (x <= 10.0) return 5;
  if (x <= 20.0) return 6;
  if (x <= 30.0) return 7;
  if (x <= 40.0) return 8;
  if (x <= 50.0) return 9;
  if (x <= 75.0) return 10;
  if (x <= 100.0) return 11;
  if (x <= 150.0) return 12;
  if (x <= 300.0) return 13;
  if (x <= 1000.0) return 14;
  if (x <= 2000.0) return 15;
  if (x <= 5000.0) return 16;
  if (x <= 10000.0) return 17;
  if (x <= 15000.0) return 18;
  return 19;
}

inline const char* baseWinRangeName(int i) {
  static const char* names[kBaseWinRangeCount] = {
    "No win", "Under 1x", "1x to 5x", "5x to 20x",
    "20x to 100x", "100x to 1,000x", "1,000x+"
  };
  if (i < 0 || i >= kBaseWinRangeCount) return "?";
  return names[i];
}

inline int baseWinRangeIndex(double x) {
  if (x <= 0.0) return 0;
  if (x < 1.0) return 1;
  if (x < 5.0) return 2;
  if (x < 20.0) return 3;
  if (x < 100.0) return 4;
  if (x < 1000.0) return 5;
  return 6;
}

inline std::string formatStakeX(double x) {
  char buf[48];
  if (std::abs(x - std::round(x)) < 1e-9)
    std::snprintf(buf, sizeof(buf), "%.0fx", x);
  else
    std::snprintf(buf, sizeof(buf), "%.2fx", x);
  return std::string(buf);
}

} // namespace bambootower
