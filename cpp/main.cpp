#include "sim.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace bambootower;

static void usage(const char* argv0) {
  std::cerr
    << "Usage: " << argv0
    << " [--mode main|respin|cashfall] [--spins N] [--seed S] [--threads T] [--out FILE]\n"
    << "  --mode     main (default) | respin (coin-respin pack) | cashfall (bonus buy/force)\n"
    << "             Each mode loads its own reelsetW pack; reelset defs shared.\n"
    << "  --spins    Number of paid rounds (default: 100000)\n"
    << "  --seed     RNG seed (default: 1)\n"
    << "  --threads  Worker threads (default: hardware concurrency; 0 = auto)\n"
    << "  --out      Stats dump (default: results/bamboo_tower_{mode}_stats.txt; '-' = stdout only)\n"
    << "\n"
    << "Bamboo Tower — Coin Collector · Cash-Fall Towers (foundation stub).\n"
    << "  Base: 5×3 · 30 lines · held coins · Collector → Coin Respin.\n"
    << "  6+ held coins → Cash-Fall Bonus (per-reel towers 3–7).\n"
    << "  Weight packs intentionally empty until authored — RTP expected 0.\n";
}

static std::string defaultStatsPath(SimMode mode) {
  return std::string("results/bamboo_tower_") + modeName(mode) + "_stats.txt";
}

static bool writeStatsFile(const std::string& path, const std::string& body) {
  namespace fs = std::filesystem;
  const fs::path p(path);
  if (p.has_parent_path() && !p.parent_path().empty()) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    if (ec) {
      std::cerr << "Failed to create directory " << p.parent_path().string()
                << ": " << ec.message() << "\n";
      return false;
    }
  }
  std::ofstream f(path);
  if (!f) {
    std::cerr << "Failed to write " << path << "\n";
    return false;
  }
  f << body;
  if (!f) {
    std::cerr << "Failed to write " << path << "\n";
    return false;
  }
  return true;
}

[[noreturn]] static void badValue(const char* flag, const std::string& s) {
  std::cerr << "Invalid value for " << flag << ": '" << s << "'\n";
  std::exit(1);
}

static long long parseLL(const char* flag, const std::string& s) {
  try {
    size_t used = 0;
    const long long v = std::stoll(s, &used);
    if (used == s.size()) return v;
  } catch (const std::exception&) {
  }
  badValue(flag, s);
}

static unsigned long long parseULL(const char* flag, const std::string& s) {
  try {
    size_t used = 0;
    const unsigned long long v = std::stoull(s, &used);
    if (used == s.size()) return v;
  } catch (const std::exception&) {
  }
  badValue(flag, s);
}

static bool parseMode(const std::string& s, SimMode& out) {
  if (s == "main") { out = SimMode::Main; return true; }
  if (s == "respin") { out = SimMode::Respin; return true; }
  if (s == "cashfall" || s == "bonus" || s == "bb") {
    out = SimMode::CashFall;
    return true;
  }
  return false;
}

int main(int argc, char** argv) {
  SimOptions opts;
  std::string outPath;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char* flag) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        usage(argv[0]);
        std::exit(1);
      }
      return argv[++i];
    };
    if (a == "--mode") {
      if (!parseMode(need("--mode"), opts.mode)) {
        std::cerr << "Unknown mode\n";
        usage(argv[0]);
        return 1;
      }
    } else if (a == "--spins") {
      opts.spins = parseLL("--spins", need("--spins"));
    } else if (a == "--seed") {
      opts.seed = static_cast<std::uint64_t>(parseULL("--seed", need("--seed")));
    } else if (a == "--threads" || a == "-j") {
      opts.threads = static_cast<int>(parseLL("--threads", need("--threads")));
    } else if (a == "--out") {
      outPath = need("--out");
    } else if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown arg: " << a << "\n";
      usage(argv[0]);
      return 1;
    }
  }

  opts.cfg = Config::forMode(opts.mode);

  if (opts.spins <= 0) {
    std::cerr << "--spins must be > 0\n";
    return 1;
  }
  if (opts.threads < 0) {
    std::cerr << "--threads must be >= 0 (0 = auto)\n";
    return 1;
  }

  (void)PAYLINES().size();

  SimReport report = runSim(opts);
  const std::string body = formatReport(report);
  std::cout << body;

  if (outPath != "-") {
    if (outPath.empty()) outPath = defaultStatsPath(opts.mode);
    if (!writeStatsFile(outPath, body)) return 1;
    std::cerr << "Wrote stats to " << outPath << "\n";
  }
  return 0;
}
