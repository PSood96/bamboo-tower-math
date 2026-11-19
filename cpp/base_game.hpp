#pragma once

/**
 * Bamboo Tower base game — board gen + line eval + Coin Respin scaffolding.
 *
 * Base board-gen order (user-specified):
 *   1. Empty board
 *   2. Roll collector land (Config::pCollectorLand — not a reel weight table)
 *   3. If yes, place one collector on a random empty cell
 *   4. Roll coin count 0..6 (Config::coinCountW)
 *   5. Place that many coins on empty cells; cash value int [coinValueMin, coinValueMax]
 *   6. Fill remaining EM cells with normal symbols (pays + wild) from reelsets
 *
 * Coins never overwrite the collector. With empty reel packs, step 6 leaves EM
 * (preference: no weight-table edits). Coin Respin / Cash-Fall loops still stubbed.
 */

#include "config.hpp"
#include "rng.hpp"
#include "stats.hpp"
#include "types.hpp"

#include <utility>
#include <vector>

namespace bambootower {

struct LineEvalResult {
  double totalX = 0.0; // × total bet
  std::vector<LineWin> wins;
};

struct BoardGenResult {
  Board board{};
  bool collectorLanded = false;
  int collectorCol = -1;
  int collectorRow = -1;
  int coinCountRolled = 0;  // requested before clamp to free cells
  int coinsPlaced = 0;
};

/**
 * Evaluate 30 fixed paylines on a base board (proto evaluateLines).
 * Pays are × total bet. All-wild run pays as top symbol H1 (FIRE7).
 */
inline LineEvalResult evaluateLines(const Board& board, const Config& cfg) {
  LineEvalResult out;
  const auto& lines = PAYLINES();
  for (size_t li = 0; li < lines.size(); ++li) {
    const auto& rowPerReel = lines[li];
    SymId symbols[COLS];
    for (int c = 0; c < COLS; ++c) {
      const int r = rowPerReel[static_cast<size_t>(c)];
      symbols[c] = board[static_cast<size_t>(c)][static_cast<size_t>(r)].sym;
    }

    SymId anchor = SymId::EM;
    for (int c = 0; c < COLS; ++c) {
      if (!isWild(symbols[c])) {
        anchor = symbols[c];
        break;
      }
    }
    if (anchor == SymId::EM) anchor = SymId::H1; // all wild → top pay
    if (!isPaySym(anchor)) continue;             // feature anchor → no line

    int runLen = 0;
    for (int c = 0; c < COLS; ++c) {
      if (symbols[c] == anchor || isWild(symbols[c])) ++runLen;
      else break;
    }
    if (runLen < 3) continue;

    const double payX = paytableTotalBet(anchor, runLen);
    if (payX <= 0.0) continue;

    LineWin w;
    w.payline = static_cast<int>(li) + 1;
    w.symbol = anchor;
    w.oak = runLen;
    w.payX = payX;
    out.wins.push_back(w);
    out.totalX = round2(out.totalX + payX);
  }
  (void)cfg;
  return out;
}

inline Board blankBoard(SymId fill = SymId::EM) {
  Board g{};
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      g[static_cast<size_t>(c)][static_cast<size_t>(r)].sym = fill;
  return g;
}

/** Count cells with a given symbol. */
inline int countSym(const Board& g, SymId id) {
  int n = 0;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      if (g[static_cast<size_t>(c)][static_cast<size_t>(r)].sym == id) ++n;
  return n;
}

/** Empty (EM) cell coordinates — collector/coin placement targets. */
inline std::vector<std::pair<int, int>> emptyCells(const Board& g) {
  std::vector<std::pair<int, int>> out;
  out.reserve(static_cast<size_t>(COLS * ROWS));
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      if (g[static_cast<size_t>(c)][static_cast<size_t>(r)].sym == SymId::EM)
        out.emplace_back(c, r);
  return out;
}

/**
 * Fill remaining EM cells with pays + wild only.
 * Uses reelset column weights when present; otherwise scaffold pool
 * (`pickScaffoldNormal`) — does not edit REELSETS() weight tables.
 * Skips held cells.
 */
inline void fillRemainingNormals(Board& g, Rng& rng, const Config& cfg) {
  const Reelset& rs = pickReelset(rng, cfg.pack);
  for (int c = 0; c < COLS; ++c) {
    for (int r = 0; r < ROWS; ++r) {
      Cell& cell = g[static_cast<size_t>(c)][static_cast<size_t>(r)];
      if (cell.held) continue;
      if (cell.sym != SymId::EM) continue;
      cell.sym = pickNormalFromReel(rng, rs, c);
      cell.coinValue = 0.0;
      cell.multHits = 0;
      cell.rolledMult = 0;
    }
  }
}

/**
 * Legacy open-cell fill (all reel draws). Prefer genBaseBoard for base spins.
 * Held cells skipped. Empty full-draw columns stay EM (scaffold is normals-only).
 */
inline void fillOpenCells(Board& g, Rng& rng, const Config& cfg) {
  const Reelset& rs = pickReelset(rng, cfg.pack);
  for (int c = 0; c < COLS; ++c) {
    for (int r = 0; r < ROWS; ++r) {
      Cell& cell = g[static_cast<size_t>(c)][static_cast<size_t>(r)];
      if (cell.held) continue;
      cell.sym = pickFromReel(rng, rs, c);
      cell.coinValue = 0.0;
      cell.multHits = 0;
      cell.rolledMult = 0;
      if (isCoin(cell.sym)) cell.coinValue = pickCoinValue(rng);
      if (isMult(cell.sym)) cell.rolledMult = pickMultValue(rng);
    }
  }
}

/**
 * Base board generation — ordered flow.
 * Driven by Config knobs + reelsets; does not edit weight tables.
 */
inline BoardGenResult genBaseBoard(Rng& rng, const Config& cfg) {
  BoardGenResult out;

  // 1. Empty board
  out.board = blankBoard(SymId::EM);

  // 2–3. Collector land?
  out.collectorLanded = rng.chance(cfg.pCollectorLand);
  if (out.collectorLanded) {
    auto free = emptyCells(out.board);
    if (!free.empty()) {
      const auto& pos = free[static_cast<size_t>(rng.nextInt(static_cast<int>(free.size())))];
      out.collectorCol = pos.first;
      out.collectorRow = pos.second;
      Cell& cell = out.board[static_cast<size_t>(pos.first)][static_cast<size_t>(pos.second)];
      cell.sym = SymId::CL;
      cell.coinValue = 0.0;
      cell.multHits = 0;
      cell.rolledMult = 0;
      cell.held = false;
    } else {
      out.collectorLanded = false;
    }
  }

  // 4. Coin count 0..6 (clamp to remaining empty cells)
  out.coinCountRolled = pickCoinCount(rng, cfg.coinCountW);
  if (out.coinCountRolled < 0) out.coinCountRolled = 0;
  if (out.coinCountRolled > 6) out.coinCountRolled = 6;

  // 5. Place coins on empty cells only (never overwrites collector)
  auto free = emptyCells(out.board);
  int toPlace = out.coinCountRolled;
  if (toPlace > static_cast<int>(free.size())) toPlace = static_cast<int>(free.size());
  rng.shuffle(free);
  for (int i = 0; i < toPlace; ++i) {
    const int c = free[static_cast<size_t>(i)].first;
    const int r = free[static_cast<size_t>(i)].second;
    Cell& cell = out.board[static_cast<size_t>(c)][static_cast<size_t>(r)];
    cell.sym = SymId::CN;
    cell.coinValue = static_cast<double>(
        pickBoardCoinCashValue(rng, cfg.coinValueMin, cfg.coinValueMax));
    cell.multHits = 0;
    cell.rolledMult = 0;
    cell.held = false;
    ++out.coinsPlaced;
  }

  // 6. Fill remaining with normal symbols (reelset or scaffold fallback)
  fillRemainingNormals(out.board, rng, cfg);

  return out;
}

/**
 * Boost one random eligible coin (hits < maxMultHits). Returns false if none.
 * Stub helper for Coin Respin / Cash-Fall multiplier resolution.
 */
inline bool boostRandomCoin(Rng& rng, std::vector<Coin*>& coins, int maxHits) {
  std::vector<Coin*> eligible;
  for (Coin* c : coins)
    if (c && c->hits < maxHits) eligible.push_back(c);
  if (eligible.empty()) return false;
  Coin* pick = rng.pick(eligible);
  (void)pick;
  return true;
}

inline bool boostRandomCoin(Rng& rng, std::vector<Coin*>& coins, int maxHits, int mult) {
  std::vector<Coin*> eligible;
  for (Coin* c : coins)
    if (c && c->hits < maxHits) eligible.push_back(c);
  if (eligible.empty()) return false;
  Coin* pick = rng.pick(eligible);
  pick->value = round2(pick->value * static_cast<double>(mult));
  pick->hits++;
  return true;
}

/**
 * One paid base round.
 * Board gen is live; Coin Respin / Cash-Fall feature loops still stubbed.
 */
inline RoundResult playBaseRound(Rng& rng, const Config& cfg, Stats& st) {
  RoundResult res;
  const BoardGenResult gen = genBaseBoard(rng, cfg);
  res.board = gen.board;
  res.heldAtEnd = gen.coinsPlaced; // provisional until hold/respin wiring

  const LineEvalResult lines = evaluateLines(gen.board, cfg);
  res.lineWin = round2(lines.totalX * cfg.bet);
  st.baseWon = round2(st.baseWon + res.lineWin);

  // Feature loops (Coin Respin / Cash-Fall) stubbed — see bonus_game.hpp.
  res.total = round2(res.lineWin + res.collectWin + res.bonusWin);
  res.totalX = cfg.bet > 0.0 ? round2(res.total / cfg.bet) : 0.0;
  if (res.totalX > cfg.maxWinX) {
    res.totalX = cfg.maxWinX;
    res.total = round2(res.totalX * cfg.bet);
    res.winCapReached = true;
  }
  return res;
}

} // namespace bambootower
