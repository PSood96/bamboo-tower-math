#pragma once

#include "base_game.hpp"
#include "bonus_game.hpp"
#include "config.hpp"
#include "rng.hpp"
#include "stats.hpp"
#include "types.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace bambootower {

struct SimOptions {
  SimMode mode = SimMode::Main;
  long long spins = 100000;
  std::uint64_t seed = 1;
  /** Worker threads (0 = hardware_concurrency, min 1). */
  int threads = 0;
  Config cfg = Config::defaults();
};

struct SimReport {
  SimMode mode = SimMode::Main;
  long long spins = 0;
  double stakePerSpin = 1.0;
  double staked = 0.0;
  double totalWon = 0.0;
  double baseWon = 0.0;
  double collectWon = 0.0;
  double featureWon = 0.0;
  double rtp = 0.0;
  double meanX = 0.0;
  double hitRate = 0.0;
  double coinRespinRate = 0.0;
  double cashFallRate = 0.0;
  double bestX = 0.0;
  double biggestFeatX = 0.0;
  double seconds = 0.0;
  Stats detail{};
};

inline double stakeForMode(SimMode /*mode*/, const Config& cfg) {
  // Main / respin-pack / cashfall-buy stakes diverge later if buys land.
  return cfg.bet;
}

inline void printProgress(int pct, long long spinsDone, long long totalSpins, const Stats& st,
                          double stake, std::chrono::steady_clock::time_point t0, int threads) {
  auto now = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(now - t0).count();
  double totalWon = st.baseWon + st.collectWon + st.featureWon;
  double totalRtp = st.staked > 0 ? 100.0 * totalWon / st.staked : 0.0;
  double baseRtp = st.staked > 0 ? 100.0 * st.baseWon / st.staked : 0.0;
  double bonusRtp = st.staked > 0 ? 100.0 * st.featureWon / st.staked : 0.0;
  double hf = st.spins > 0 ? 100.0 * static_cast<double>(st.hits) / st.spins : 0.0;
  double rate = elapsed > 0.0 ? static_cast<double>(spinsDone) / elapsed : 0.0;

  std::cerr << std::fixed << std::setprecision(2)
            << "[" << std::setw(3) << pct << "%]"
            << "  elapsed=" << std::setprecision(1) << elapsed << "s"
            << "  spins=" << spinsDone << "/" << totalSpins
            << std::setprecision(2)
            << "  Total RTP=" << totalRtp << "%"
            << "  Base RTP=" << baseRtp << "%"
            << "  Bonus RTP=" << bonusRtp << "%"
            << "  HF=" << hf << "%"
            << "  (stake/spin=" << stake << "×"
            << "  threads=" << threads
            << "  " << std::setprecision(0) << rate << " spins/s)\n"
            << std::flush;
}

/**
 * One paid round.
 * Main: base (stub) — later adds Coin Respin / natural Cash-Fall.
 * CashFall: jump straight into bonus stub (buy / force path).
 * Respin: runs under respin weight pack for pack-isolation smoke sims.
 */
inline RoundResult playSpin(SimMode mode, Rng& rng, const Config& cfg, Stats& st) {
  RoundResult res;
  if (mode == SimMode::CashFall) {
    auto feat = playCashFallBuy(rng, cfg, st, cfg.maxWinX);
    res.featureX = feat.boardX;
    res.bonusWin = round2(feat.boardX * cfg.bet);
    res.cashFall = true;
    st.featureWon = round2(st.featureWon + res.bonusWin);
    st.cashFallTrig++;
    st.featureTrig++;
    res.totalX = feat.boardX;
    res.total = res.bonusWin;
    if (feat.winCapReached) {
      res.winCapReached = true;
      st.maxWinCap.hits++;
      st.maxWinCap.won = round2(st.maxWinCap.won + res.total);
    }
  } else {
    res = playBaseRound(rng, cfg, st);
  }
  return res;
}

inline std::uint64_t workerSeed(std::uint64_t seed, unsigned tid) {
  std::uint64_t x = seed + 0x9E3779B97F4A7C15ULL * (static_cast<std::uint64_t>(tid) + 1ULL);
  x ^= x >> 30;
  x *= 0xBF58476D1CE4E5B9ULL;
  x ^= x >> 27;
  x *= 0x94D049BB133111EBULL;
  x ^= x >> 31;
  return x ? x : 1ULL;
}

inline void runWorkerSpins(SimMode mode, const Config& cfg, double stake, double stakeX,
                           long long spins, std::uint64_t seed, Stats& st,
                           std::atomic<long long>* progress,
                           const std::function<void(long long done, const Stats&)>* onTick) {
  Rng rng(seed);
  st.reset();
  st.stakeX = stakeX;
  constexpr long long kProgressChunk = 2048;

  for (long long n = 0; n < spins; ++n) {
    st.spins++;
    st.staked = round2(st.staked + stake);

    RoundResult res = playSpin(mode, rng, cfg, st);

    if (res.total > 0) st.hits++;
    if (res.lineWin > 0) st.baseHits++;
    if (res.coinRespin) st.coinRespinTrig++;
    {
      const double baseX = cfg.bet > 0.0 ? res.lineWin / cfg.bet : 0.0;
      st.recordSpin(res.totalX, baseX, res.featureX);
    }

    const long long done = n + 1;
    if (progress && (done % kProgressChunk == 0 || done == spins)) {
      const long long chunk =
          (done % kProgressChunk == 0) ? kProgressChunk : (done % kProgressChunk);
      progress->fetch_add(chunk, std::memory_order_relaxed);
    }
    if (onTick && (done % kProgressChunk == 0 || done == spins))
      (*onTick)(done, st);
  }
  st.finalizeStreaks();
}

inline SimReport runSim(const SimOptions& opts) {
  const Config& cfg = opts.cfg;
  double stake = stakeForMode(opts.mode, cfg);
  double stakeX = (cfg.bet > 0.0) ? (stake / cfg.bet) : 1.0;
  if (stakeX <= 0.0) stakeX = 1.0;

  unsigned hc = std::thread::hardware_concurrency();
  if (hc == 0) hc = 1;
  int threads = opts.threads;
  if (threads <= 0) threads = static_cast<int>(hc);
  if (threads < 1) threads = 1;
  if (static_cast<long long>(threads) > opts.spins)
    threads = static_cast<int>(opts.spins);

  auto t0 = std::chrono::steady_clock::now();
  const long long total = opts.spins;

  std::cerr << "Sim start  mode=" << modeName(opts.mode)
            << "  spins=" << total
            << "  seed=" << opts.seed
            << "  stake/spin=" << stake << "×"
            << "  threads=" << threads
            << "\n"
            << std::flush;

  Stats st{};
  st.stakeX = stakeX;
  std::atomic<long long> progress{0};

  if (threads == 1) {
    int nextPct = 10;
    std::function<void(long long, const Stats&)> tick =
        [&](long long done, const Stats& local) {
          int pct = total > 0 ? static_cast<int>((100.0 * done) / total) : 100;
          while (nextPct <= 100 && pct >= nextPct) {
            printProgress(nextPct, done, total, local, stake, t0, 1);
            nextPct += 10;
          }
        };
    runWorkerSpins(opts.mode, cfg, stake, stakeX, total, opts.seed, st, nullptr, &tick);
    if (nextPct <= 100) printProgress(100, st.spins, total, st, stake, t0, 1);
  } else {
    std::vector<Stats> locals(static_cast<size_t>(threads));
    std::vector<std::thread> pool;

    long long base = total / threads;
    long long rem = total % threads;
    for (int t = 0; t < threads; ++t) {
      long long n = base + (t < rem ? 1 : 0);
      if (n <= 0) continue;
      pool.emplace_back([&, t, n]() {
        runWorkerSpins(opts.mode, cfg, stake, stakeX, n,
                       workerSeed(opts.seed, static_cast<unsigned>(t)),
                       locals[static_cast<size_t>(t)], &progress, nullptr);
      });
    }

    int nextPct = 10;
    while (true) {
      long long done = progress.load(std::memory_order_relaxed);
      if (done >= total) break;
      int pct = total > 0 ? static_cast<int>((100.0 * done) / total) : 100;
      while (nextPct < 100 && pct >= nextPct) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - t0).count();
        double rate = elapsed > 0.0 ? static_cast<double>(done) / elapsed : 0.0;
        std::cerr << std::fixed << std::setprecision(1)
                  << "[" << std::setw(3) << nextPct << "%]"
                  << "  elapsed=" << elapsed << "s"
                  << "  spins=" << done << "/" << total
                  << "  threads=" << threads
                  << "  " << std::setprecision(0) << rate << " spins/s\n"
                  << std::flush;
        nextPct += 10;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    for (auto& th : pool) th.join();
    for (auto& local : locals) st.add(local);
    printProgress(100, st.spins, total, st, stake, t0, threads);
  }

  auto t1 = std::chrono::steady_clock::now();
  double seconds = std::chrono::duration<double>(t1 - t0).count();

  SimReport r;
  r.mode = opts.mode;
  r.spins = st.spins;
  r.stakePerSpin = stake;
  r.staked = st.staked;
  r.baseWon = st.baseWon;
  r.collectWon = st.collectWon;
  r.featureWon = st.featureWon;
  r.totalWon = round2(st.baseWon + st.collectWon + st.featureWon);
  r.rtp = r.staked > 0 ? r.totalWon / r.staked : 0.0;
  r.meanX = st.spins > 0 ? (r.totalWon / cfg.bet) / st.spins : 0.0;
  r.hitRate = st.spins > 0 ? static_cast<double>(st.hits) / st.spins : 0.0;
  r.coinRespinRate =
      st.spins > 0 ? static_cast<double>(st.coinRespinTrig) / st.spins : 0.0;
  r.cashFallRate =
      st.spins > 0 ? static_cast<double>(st.cashFallTrig) / st.spins : 0.0;
  r.bestX = st.bestX;
  r.biggestFeatX = st.biggestFeatX;
  r.seconds = seconds;
  r.detail = st;
  return r;
}

inline void csvEscape(std::ostringstream& o, const std::string& s) {
  bool needQuotes = s.find_first_of(",\"\n\r") != std::string::npos;
  if (!needQuotes) {
    o << s;
    return;
  }
  o << '"';
  for (char c : s) {
    if (c == '"') o << "\"\"";
    else o << c;
  }
  o << '"';
}

inline void csvRow(std::ostringstream& o, const std::string& section, const std::string& label,
                   long long hits, double oddsPct, double avgX, double rtpPct) {
  csvEscape(o, section);
  o << ',';
  csvEscape(o, label);
  o << ',' << hits << ',' << std::fixed << std::setprecision(6)
    << oddsPct << ',' << avgX << ',' << rtpPct << '\n';
}

inline std::string formatReport(const SimReport& r) {
  const Stats& st = r.detail;
  std::ostringstream o;
  o << "section,label,hits,odds_pct,avg_x,rtp_pct\n";

  const double spins = static_cast<double>(r.spins > 0 ? r.spins : 1);
  auto odds = [&](long long h) { return 100.0 * static_cast<double>(h) / spins; };
  auto avg = [&](long long h, double wonCredits) {
    return h > 0 ? (wonCredits / static_cast<double>(h)) / r.stakePerSpin : 0.0;
  };
  auto rtp = [&](double wonCredits) {
    return r.staked > 0 ? 100.0 * wonCredits / r.staked : 0.0;
  };

  csvRow(o, "summary", "spins", r.spins, 100.0, r.meanX, 100.0 * r.rtp);
  csvRow(o, "summary", "total", r.spins, 100.0, r.meanX, 100.0 * r.rtp);
  csvRow(o, "summary", "base_lines", st.baseHits, odds(st.baseHits),
         avg(st.baseHits, st.baseWon), rtp(st.baseWon));
  csvRow(o, "summary", "collect", st.collectTrig, odds(st.collectTrig),
         avg(st.collectTrig, st.collectWon), rtp(st.collectWon));
  csvRow(o, "summary", "cashfall", st.cashFallTrig, odds(st.cashFallTrig),
         avg(st.cashFallTrig, st.featureWon), rtp(st.featureWon));
  csvRow(o, "summary", "coin_respin", st.coinRespinTrig, odds(st.coinRespinTrig), 0.0, 0.0);

  for (int i = 0; i < kWinRangeCount; ++i) {
    csvRow(o, "win_range", winRangeName(i), st.winRange[i].hits, odds(st.winRange[i].hits),
           avg(st.winRange[i].hits, st.winRange[i].won), rtp(st.winRange[i].won));
  }
  for (int i = 0; i < kBaseWinRangeCount; ++i) {
    csvRow(o, "base_win_range", baseWinRangeName(i), st.baseWinRange[i].hits,
           odds(st.baseWinRange[i].hits),
           avg(st.baseWinRange[i].hits, st.baseWinRange[i].won),
           rtp(st.baseWinRange[i].won));
  }

  o << "meta,mode," << modeName(r.mode) << ",,,,," << "\n";
  o << "meta,seconds," << std::fixed << std::setprecision(3) << r.seconds << ",,,,," << "\n";
  o << "meta,note,foundation_stub_empty_packs_rtp_expected_0,,,,,\n";
  return o.str();
}

} // namespace bambootower
