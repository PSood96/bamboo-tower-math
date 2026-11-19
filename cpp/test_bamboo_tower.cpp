/**
 * Lightweight sanity checks for Bamboo Tower math (wild-descent shape).
 * No GoogleTest — plain asserts against headers used by the sim.
 *
 * Build & run (from cpp/):
 *   c++ -std=c++17 -O2 -o test_bamboo_tower test_bamboo_tower.cpp && ./test_bamboo_tower
 *
 * Or from repo root:
 *   ./scripts/run_sanity_checks.sh
 */

#include "base_game.hpp"
#include "bonus_game.hpp"
#include "config.hpp"
#include "rng.hpp"
#include "types.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace bambootower;

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg));     \
      ++g_failed;                                                              \
    } else {                                                                   \
      ++g_passed;                                                              \
    }                                                                          \
  } while (0)

static Cell pay(SymId s) {
  Cell c;
  c.sym = s;
  return c;
}

static Cell wild() {
  Cell c;
  c.sym = SymId::W;
  return c;
}

static Cell coin(double v = 1.0) {
  Cell c;
  c.sym = SymId::CN;
  c.coinValue = v;
  return c;
}

static Board blankPay(SymId fill = SymId::L5) {
  Board g{};
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      g[static_cast<size_t>(c)][static_cast<size_t>(r)] = pay(fill);
  return g;
}

// -----------------------------------------------------------------------------
// Symbol list / reel hygiene
// -----------------------------------------------------------------------------

static void test_board_geometry() {
  CHECK(COLS == 5 && ROWS == 3, "base board is 5×3");
  CHECK(BONUS_ROWS == 7, "bonus grid grows to 7 rows");
  CHECK(LINE_COUNT == 30, "30 fixed paylines");
  CHECK(TOWER_H[0] == 3 && TOWER_H[4] == 7, "tower heights 3..7");
  CHECK(HELD_BONUS_TRIGGER == 6, "6+ held coins trigger Cash-Fall");
}

static void test_symbol_list_basics() {
  CHECK(isPaySym(SymId::H1) && isPaySym(SymId::L5), "H1–L5 are pay symbols");
  CHECK(isHigh(SymId::H1) && isLow(SymId::L5), "high/low split");
  CHECK(!isPaySym(SymId::W) && !isPaySym(SymId::CN) && !isPaySym(SymId::EM),
        "W/CN/EM are not pay symbols");
  CHECK(isWild(SymId::W) && isCoin(SymId::CN) && isCollector(SymId::CL),
        "wild/coin/collector recognized");
  CHECK(isFeatureSym(SymId::CN) && isFeatureSym(SymId::MU) && isFeatureSym(SymId::HT),
        "feature symbols flagged");
  CHECK(isStripToken(SymId::W), "wild is a strip token");
  CHECK(isStripToken(SymId::CN) && isStripToken(SymId::BL), "CN/BL strip tokens");
  CHECK(!isStripToken(SymId::EM), "EM is engine-only, not a strip token");
  CHECK(std::string(symName(SymId::W)) == "WW", "wild prints as WW");
  CHECK(std::string(symName(SymId::CN)) == "CN", "coin prints as CN");
}

static void test_parse_sym_names() {
  SymId id{};
  CHECK(parseSymName("H3", id) && id == SymId::H3, "parse H3");
  CHECK(parseSymName("FIRE7", id) && id == SymId::H1, "parse FIRE7 alias");
  CHECK(parseSymName("BELL", id) && id == SymId::H2, "parse BELL alias");
  CHECK(parseSymName("10", id) && id == SymId::L5, "parse 10 alias");
  CHECK(parseSymName("WW", id) && id == SymId::W, "parse WW");
  CHECK(parseSymName("WILD", id) && id == SymId::W, "parse WILD");
  CHECK(parseSymName("COIN", id) && id == SymId::CN, "parse COIN");
  CHECK(parseSymName("COLL", id) && id == SymId::CL, "parse COLL");
  CHECK(parseSymName("BLANK", id) && id == SymId::BL, "parse BLANK");
  CHECK(parseSymName("HEART", id) && id == SymId::HT, "parse HEART");
  CHECK(!parseSymName("1L", id), "reject typo 1L");
  CHECK(!parseSymName("l2", id), "reject typo l2");
  CHECK(!parseSymName("EM", id), "reject EM as strip spelling");
  CHECK(!parseSymName("SC", id), "reject wild-descent SC leftover");
  CHECK(!parseSymName("XX", id), "reject unknown token");
}

static void test_reelsets_registered() {
  CHECK(REELSETS().size() == 3, "Main1/Respin1/Bonus1 registered");
  CHECK(REELSETS()[0].id == "Main1", "first reelset is Main1");
  CHECK(reelsetById("Respin1").id == "Respin1", "reelsetById finds Respin1");
  CHECK(reelsetById("Bonus1").id == "Bonus1", "reelsetById finds Bonus1");
}

static void test_odd_reel_tokens_empty() {
  const auto odd = oddReelTokens();
  CHECK(odd.empty(), "no landable reel tokens outside the symbol list");
  if (!odd.empty()) {
    for (const auto& t : odd)
      std::fprintf(stderr, "  odd token: %s\n", t.c_str());
  }
}

static void test_reelset_weights_non_negative() {
  for (const auto& rs : REELSETS()) {
    for (int di = 0; di < kReelDrawCount; ++di) {
      for (int c = 0; c < COLS; ++c) {
        const double w = rs.w[static_cast<size_t>(di)][static_cast<size_t>(c)];
        CHECK(std::isfinite(w) && w >= 0.0,
              "reelset cell weights must be finite and >= 0");
      }
    }
  }
}

static void test_mode_weight_pack_structure() {
  for (SimMode m : {SimMode::Main, SimMode::Respin, SimMode::CashFall}) {
    const ModeWeightPack pack = weightPackForMode(m);
    CHECK(pack.reelsetW.empty(), "blank mode reelsetW stays empty until authored");
    for (const auto& p : pack.reelsetW) {
      CHECK(std::isfinite(p.second) && p.second >= 0.0,
            "mode reelsetW weights finite and >= 0");
      bool found = false;
      for (const auto& rs : REELSETS())
        if (rs.id == p.first) found = true;
      CHECK(found, "reelsetW id must exist in REELSETS()");
    }
  }
}

static void test_paytable_scaffold() {
  CHECK(paytableTotalBet(SymId::H1, 5) == 9.39, "H1/FIRE7 5oak matches proto");
  CHECK(paytableTotalBet(SymId::L5, 3) == 0.09, "L5/10 3oak matches proto");
  CHECK(paytableTotalBet(SymId::W, 5) == 0.0, "wild has no paytable line");
  CHECK(paytableTotalBet(SymId::CN, 3) == 0.0, "coin has no paytable line");
  CHECK(paytableTotalBet(SymId::H1, 2) == 0.0, "oak < 3 pays 0");
  CHECK(paytableTotalBet(SymId::H1, 6) == 9.39, "oak > 5 uses 5oak row");
  CHECK(paytableLineBet(SymId::H2, 4) == 1.25, "lineBet alias matches totalBet");
}

static void test_paylines_count() {
  CHECK(static_cast<int>(PAYLINES().size()) == LINE_COUNT, "PAYLINES size == 30");
  for (const auto& line : PAYLINES()) {
    for (int c = 0; c < COLS; ++c) {
      CHECK(line[static_cast<size_t>(c)] >= 0 && line[static_cast<size_t>(c)] < ROWS,
            "payline row index in [0,ROWS)");
    }
  }
}

static void test_config_defaults() {
  const Config cfg = Config::defaults();
  CHECK(cfg.pack.reelsetW.empty(), "defaults pack matches blank Main");
  CHECK(cfg.heldBonusTrigger == 6, "held trigger 6");
  CHECK(cfg.baseRespinStart == 3, "base respin start 3");
  CHECK(cfg.bonusReelSpins == 3, "bonus reel spins 3");
  CHECK(cfg.pCollectorLand == 0.0, "default pCollectorLand off");
  CHECK(cfg.coinCountW[0] == 1.0, "default coin count mass on 0");
  CHECK(cfg.coinValueMin == 1 && cfg.coinValueMax == 100, "default coin cash 1–100");
  CHECK(modeName(SimMode::Main) == std::string("main"), "modeName main");
  CHECK(modeName(SimMode::Respin) == std::string("respin"), "modeName respin");
  CHECK(modeName(SimMode::CashFall) == std::string("cashfall"), "modeName cashfall");
}

static void test_rng_unit_range() {
  Rng rng(42);
  for (int i = 0; i < 100; ++i) {
    const double u = rng.nextDouble();
    CHECK(u >= 0.0 && u < 1.0, "rng nextDouble in [0,1)");
  }
  CHECK(rng.chance(0.0) == false, "chance(0) false without draw semantics ok");
  CHECK(rng.chance(1.0) == true, "chance(1) true");
  CHECK(rng.nextInt(1) == 0, "nextInt(1) → 0");
  std::vector<std::pair<int, double>> items = {{1, 1.0}, {2, 1.0}};
  const int idx = rng.pickWeightedIndex(items);
  CHECK(idx == 0 || idx == 1, "pickWeightedIndex returns a valid index");
  std::vector<std::pair<int, double>> empty;
  CHECK(rng.pickWeightedIndex(empty) == -1, "pickWeightedIndex empty → -1");
}

static void test_coin_mult_tables() {
  CHECK(COIN_VALUES().size() == 12, "coin value table size");
  CHECK(MULT_VALUES().size() == 10, "mult value table size");
  Rng rng(7);
  const double cv = pickCoinValue(rng);
  bool found = false;
  for (double v : COIN_VALUES())
    if (v == cv) found = true;
  CHECK(found, "pickCoinValue returns a table entry");
  const int mv = pickMultValue(rng);
  found = false;
  for (int v : MULT_VALUES())
    if (v == mv) found = true;
  CHECK(found, "pickMultValue returns a table entry");
  for (int i = 0; i < 50; ++i) {
    const int cash = pickBoardCoinCashValue(rng, 1, 100);
    CHECK(cash >= 1 && cash <= 100, "board coin cash in [1,100]");
  }
  CHECK(pickCoinCount(rng, defaultCoinCountW()) == 0, "default coinCountW → 0");
}

// -----------------------------------------------------------------------------
// Line evaluation (implemented foundation)
// -----------------------------------------------------------------------------

static void test_line_eval_five_oak() {
  Board g = blankPay(SymId::H1);
  Config cfg = Config::defaults();
  const LineEvalResult r = evaluateLines(g, cfg);
  CHECK(r.wins.size() >= 1, "full H1 board has line wins");
  bool saw5 = false;
  for (const auto& w : r.wins)
    if (w.oak == 5 && w.symbol == SymId::H1) saw5 = true;
  CHECK(saw5, "at least one 5oak H1");
  CHECK(r.totalX > 0.0, "line totalX > 0");
}

static void test_line_eval_wild_sub() {
  Board g = blankPay(SymId::L5);
  g[1][0] = wild();
  g[2][0] = wild();
  // Line 1 is top row [0,0,0,0,0] → L5 W W L5 L5 → 5oak L5
  Config cfg = Config::defaults();
  const LineEvalResult r = evaluateLines(g, cfg);
  bool saw = false;
  for (const auto& w : r.wins)
    if (w.payline == 1 && w.symbol == SymId::L5 && w.oak == 5) saw = true;
  CHECK(saw, "top line 5oak L5 with wilds");
}

static void test_line_eval_feature_breaks() {
  Board g = blankPay(SymId::H2);
  g[2][0] = coin(1.0); // breaks top line mid-run
  Config cfg = Config::defaults();
  const LineEvalResult r = evaluateLines(g, cfg);
  for (const auto& w : r.wins) {
    if (w.payline == 1) {
      CHECK(false, "top line should not pay when coin breaks run");
    }
  }
  CHECK(true, "coin breaks line run");
}

static void test_line_eval_all_wild_pays_top() {
  Board g{};
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      g[static_cast<size_t>(c)][static_cast<size_t>(r)] = wild();
  Config cfg = Config::defaults();
  const LineEvalResult r = evaluateLines(g, cfg);
  CHECK(!r.wins.empty(), "all-wild board pays");
  for (const auto& w : r.wins)
    CHECK(w.symbol == SymId::H1, "all-wild anchors as H1/FIRE7");
}

static void test_boost_random_coin() {
  Rng rng(1);
  Coin a{1.0, 0}, b{2.0, MAX_MULT_HITS};
  std::vector<Coin*> coins = {&a, &b};
  CHECK(boostRandomCoin(rng, coins, MAX_MULT_HITS, 2), "boost eligible coin");
  CHECK(a.value == 2.0 && a.hits == 1, "coin A boosted");
  CHECK(b.hits == MAX_MULT_HITS, "maxed coin untouched");
}

static void test_tower_helpers() {
  CHECK(towerCapacity(0) == 3 && towerCapacity(4) == 7, "towerCapacity");
  Config cfg = Config::defaults();
  TowerState t = makeFreshTowers(cfg);
  CHECK(t.spinsLeft[0] == 3 && !t.dead[2], "fresh towers start live with 3 spins");
}

static void test_cashfall_stub_zero() {
  Rng rng(1);
  Config cfg = Config::defaults();
  Stats st{};
  FeatureResult f = playCashFall(rng, cfg, st, cfg.maxWinX);
  CHECK(f.boardX == 0.0 && f.steps == 0, "Cash-Fall stub returns zero");
}

static void test_play_base_round_default_knobs() {
  Rng rng(1);
  Config cfg = Config::defaults();
  Stats st{};
  RoundResult res = playBaseRound(rng, cfg, st);
  CHECK(countSym(res.board, SymId::CL) == 0, "default: no collector");
  CHECK(countSym(res.board, SymId::CN) == 0, "default: no coins");
  CHECK(countSym(res.board, SymId::EM) == 0, "scaffold fills all EM cells");
  // Board is pays/wild only — may or may not win this single spin.
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r) {
      const SymId s = res.board[static_cast<size_t>(c)][static_cast<size_t>(r)].sym;
      CHECK(isPaySym(s) || isWild(s), "default board is pays/wild only");
    }
}

static void test_play_base_round_nonzero_line_wins() {
  Rng rng(1);
  Config cfg = Config::defaults();
  Stats st{};
  double won = 0.0;
  for (int i = 0; i < 3000; ++i) {
    RoundResult res = playBaseRound(rng, cfg, st);
    won += res.lineWin;
  }
  CHECK(won > 0.0, "scaffold fill → some line wins over 3000 spins");
  CHECK(st.baseWon > 0.0, "stats baseWon accumulates");
}

// -----------------------------------------------------------------------------
// Base board-gen flow
// -----------------------------------------------------------------------------

static Config boardGenTestCfg() {
  Config cfg = Config::defaults();
  cfg.pCollectorLand = 1.0;
  cfg.coinCountW = {{0, 0, 0, 1.0, 0, 0, 0}}; // always 3 coins
  cfg.coinValueMin = 1;
  cfg.coinValueMax = 100;
  return cfg;
}

static void test_gen_base_board_order_and_counts() {
  Rng rng(42);
  Config cfg = boardGenTestCfg();
  const BoardGenResult gen = genBaseBoard(rng, cfg);

  CHECK(gen.collectorLanded, "pCollectorLand=1 → collector landed");
  CHECK(countSym(gen.board, SymId::CL) == 1, "exactly one collector");
  CHECK(gen.collectorCol >= 0 && gen.collectorRow >= 0, "collector position set");
  CHECK(gen.board[static_cast<size_t>(gen.collectorCol)]
                 [static_cast<size_t>(gen.collectorRow)].sym == SymId::CL,
        "collector cell is CL");

  CHECK(gen.coinCountRolled == 3, "coinCountW forces 3");
  CHECK(gen.coinsPlaced == 3, "placed 3 coins");
  CHECK(countSym(gen.board, SymId::CN) == 3, "board has 3 CN");

  // Collector cell not overwritten by a coin
  CHECK(!isCoin(gen.board[static_cast<size_t>(gen.collectorCol)]
                         [static_cast<size_t>(gen.collectorRow)].sym),
        "coins do not overwrite collector");

  // Empty REELSETS weights → scaffold fills remaining (no EM left)
  CHECK(countSym(gen.board, SymId::EM) == 0, "scaffold fills remaining cells");
  CHECK(countSym(gen.board, SymId::CL) + countSym(gen.board, SymId::CN) == 4,
        "collector + coins preserved after fill");
}

static void test_gen_base_board_coin_values() {
  Rng rng(99);
  Config cfg = boardGenTestCfg();
  cfg.coinCountW = {{0, 0, 0, 0, 0, 0, 1.0}}; // always 6 coins
  const BoardGenResult gen = genBaseBoard(rng, cfg);
  CHECK(gen.coinsPlaced == 6, "6 coins placed");
  CHECK(countSym(gen.board, SymId::CL) == 1, "collector + 6 coins");
  for (int c = 0; c < COLS; ++c) {
    for (int r = 0; r < ROWS; ++r) {
      const Cell& cell = gen.board[static_cast<size_t>(c)][static_cast<size_t>(r)];
      if (!isCoin(cell.sym)) continue;
      CHECK(cell.coinValue >= 1.0 && cell.coinValue <= 100.0,
            "coin cash value in [1,100]");
      CHECK(cell.coinValue == static_cast<double>(static_cast<int>(cell.coinValue)),
            "coin cash value is integer");
    }
  }
}

static void test_gen_base_board_no_collector() {
  Rng rng(3);
  Config cfg = Config::defaults();
  cfg.pCollectorLand = 0.0;
  cfg.coinCountW = {{0, 0, 1.0, 0, 0, 0, 0}}; // always 2 coins
  const BoardGenResult gen = genBaseBoard(rng, cfg);
  CHECK(!gen.collectorLanded, "no collector");
  CHECK(countSym(gen.board, SymId::CL) == 0, "zero CL");
  CHECK(gen.coinsPlaced == 2 && countSym(gen.board, SymId::CN) == 2, "2 coins");
}

static void test_gen_base_board_coins_skip_collector_cells() {
  // Force collector + 6 coins on 15-cell board → 1 CL + 6 CN + 8 scaffold pays
  Rng rng(11);
  Config cfg = boardGenTestCfg();
  cfg.coinCountW = {{0, 0, 0, 0, 0, 0, 1.0}};
  const BoardGenResult gen = genBaseBoard(rng, cfg);
  CHECK(countSym(gen.board, SymId::CL) == 1, "one collector");
  CHECK(countSym(gen.board, SymId::CN) == 6, "six coins");
  int feature = 0;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r) {
      const SymId s = gen.board[static_cast<size_t>(c)][static_cast<size_t>(r)].sym;
      if (s == SymId::CL || s == SymId::CN) ++feature;
      else CHECK(isPaySym(s) || isWild(s), "remaining cells are scaffold pays/wild");
    }
  CHECK(feature == 7, "collector + coins occupy 7 cells");
  CHECK(countSym(gen.board, SymId::EM) == 0, "no EM after scaffold fill");
}

static void test_pick_normal_from_local_reelset() {
  // Local reelset for unit test only — does not edit REELSETS() weight tables.
  Reelset rs;
  rs.id = "TestPay";
  for (int c = 0; c < COLS; ++c) {
    rs.w[static_cast<size_t>(ReelDraw::L5)][static_cast<size_t>(c)] = 1.0;
    rs.w[static_cast<size_t>(ReelDraw::CN)][static_cast<size_t>(c)] = 100.0; // ignored
    rs.w[static_cast<size_t>(ReelDraw::CL)][static_cast<size_t>(c)] = 100.0; // ignored
  }
  Rng rng(5);
  for (int i = 0; i < 20; ++i) {
    const SymId s = pickNormalFromReel(rng, rs, 0);
    CHECK(s == SymId::L5, "normalsOnly skips CN/CL weights");
  }
  const SymId any = pickFromReel(rng, rs, 0, /*normalsOnly=*/false);
  CHECK(isCoin(any) || isCollector(any) || any == SymId::L5,
        "full pick can see feature weights on local set");
}

static void test_scaffold_normal_fallback() {
  Rng rng(2);
  Reelset empty = makeEmptyReelset("EmptyLocal");
  for (int i = 0; i < 40; ++i) {
    const SymId s = pickNormalFromReel(rng, empty, i % COLS);
    CHECK(isPaySym(s) || isWild(s), "empty reel → scaffold pay/wild");
    CHECK(s != SymId::EM, "scaffold never returns EM");
  }
  CHECK(pickFromReel(rng, empty, 0, /*normalsOnly=*/false) == SymId::EM,
        "full pick on empty column still EM");
}

static void test_gen_base_board_fill_with_injected_pack() {
  // Prove step 6 fills EM when a pack points at a local-weight reelset id that
  // exists in REELSETS — still without editing those shared tables' zeros.
  // With empty Main1, fill stays EM; we instead call fillRemainingNormals after
  // manually placing pays via pickNormalFromReel on a local set.
  Rng rng(8);
  Board g = blankBoard(SymId::EM);
  g[0][0].sym = SymId::CL;
  g[1][0].sym = SymId::CN;
  g[1][0].coinValue = 50;
  Reelset rs;
  rs.id = "local";
  for (int c = 0; c < COLS; ++c)
    rs.w[static_cast<size_t>(ReelDraw::H2)][static_cast<size_t>(c)] = 1.0;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r) {
      Cell& cell = g[static_cast<size_t>(c)][static_cast<size_t>(r)];
      if (cell.sym != SymId::EM) continue;
      cell.sym = pickNormalFromReel(rng, rs, c);
    }
  CHECK(countSym(g, SymId::CL) == 1, "collector preserved through fill");
  CHECK(countSym(g, SymId::CN) == 1, "coin preserved through fill");
  CHECK(countSym(g, SymId::H2) == COLS * ROWS - 2, "EM filled with H2");
  CHECK(countSym(g, SymId::EM) == 0, "no EM left");
}

int main() {
  test_board_geometry();
  test_symbol_list_basics();
  test_parse_sym_names();
  test_reelsets_registered();
  test_odd_reel_tokens_empty();
  test_reelset_weights_non_negative();
  test_mode_weight_pack_structure();
  test_paytable_scaffold();
  test_paylines_count();
  test_config_defaults();
  test_rng_unit_range();
  test_coin_mult_tables();
  test_line_eval_five_oak();
  test_line_eval_wild_sub();
  test_line_eval_feature_breaks();
  test_line_eval_all_wild_pays_top();
  test_boost_random_coin();
  test_tower_helpers();
  test_cashfall_stub_zero();
  test_play_base_round_default_knobs();
  test_play_base_round_nonzero_line_wins();
  test_gen_base_board_order_and_counts();
  test_gen_base_board_coin_values();
  test_gen_base_board_no_collector();
  test_gen_base_board_coins_skip_collector_cells();
  test_pick_normal_from_local_reelset();
  test_scaffold_normal_fallback();
  test_gen_base_board_fill_with_injected_pack();

  std::printf("Bamboo Tower sanity: %d passed, %d failed\n", g_passed, g_failed);
  return g_failed ? 1 : 0;
}
