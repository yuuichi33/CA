#include <iostream>
#include <string>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <fstream>
#include <iomanip>
#include "periph/tohost_mmio.h"
#include "cpu/pipeline.h"
#include "isa/decoder.h"

static std::atomic<bool> running{true};

static void sigint_handler(int) { running.store(false); }

int main(int argc, char** argv) {
  bool enable_uart_bridge = false;
  std::string elf_path;
  uint64_t mtvec_val = 0;
  bool mtvec_set = false;
  int timer_interval = -1;
  uint64_t timer_mtimecmp = 0;
  bool timer_mtimecmp_set = false;
  bool timer_enable = false;
  uint32_t uart_ier_val = 0;
  uint64_t run_cycles = 0;
  bool run_cycles_set = false;
  bool verbose = false;
  bool step_mode = false;
  bool pause_on_start = false;
  bool quiet = false;
  bool no_cache = false;
  int cache_penalty = 10;
  size_t cache_size = 16 * 1024;
  size_t cache_line_size = 64;
  unsigned cache_assoc = 4;
  bool cache_write_back = true;
  bool cache_write_allocate = true;
  std::string trace_json_target;
  std::vector<uint32_t> breakpoints;
  std::vector<std::string> cli_argv;
  std::vector<std::string> cli_envp;

  auto parse_u64 = [](const std::string &s)->uint64_t {
    try { return std::stoull(s, nullptr, 0); } catch (...) { return 0; }
  };

  for (int i = 1; i < argc; ++i) {
    std::string a(argv[i]);
    if (a == "--uart-stdin" || a == "-u") enable_uart_bridge = true;
    else if (a == "--load" || a == "-l") {
      if (i + 1 < argc) { elf_path = argv[++i]; }
      else { std::cerr << "--load requires a file path\n"; return 1; }
    } else if (a == "--mtvec" || a == "-m") {
      if (i + 1 < argc) { mtvec_val = parse_u64(argv[++i]); mtvec_set = true; } else { std::cerr << "--mtvec requires an address\n"; return 1; }
    } else if (a == "--timer-interval") {
      if (i + 1 < argc) { timer_interval = static_cast<int>(parse_u64(argv[++i])); } else { std::cerr << "--timer-interval requires a number\n"; return 1; }
    } else if (a == "--timer-mtimecmp") {
      if (i + 1 < argc) { timer_mtimecmp = parse_u64(argv[++i]); timer_mtimecmp_set = true; } else { std::cerr << "--timer-mtimecmp requires a value\n"; return 1; }
    } else if (a == "--timer-enable") {
      timer_enable = true;
    } else if (a == "--verbose" || a == "-v") {
      verbose = true;
    } else if (a == "--quiet" || a == "-q") {
      quiet = true;
    } else if (a == "--no-cache") {
      no_cache = true;
    } else if (a == "--cache-penalty") {
      if (i + 1 < argc) { cache_penalty = static_cast<int>(parse_u64(argv[++i])); } else { std::cerr << "--cache-penalty requires a number\n"; return 1; }
    } else if (a == "--cache-size") {
      if (i + 1 < argc) { cache_size = static_cast<size_t>(parse_u64(argv[++i])); } else { std::cerr << "--cache-size requires a number\n"; return 1; }
    } else if (a == "--cache-line-size") {
      if (i + 1 < argc) { cache_line_size = static_cast<size_t>(parse_u64(argv[++i])); } else { std::cerr << "--cache-line-size requires a number\n"; return 1; }
    } else if (a == "--cache-assoc") {
      if (i + 1 < argc) { cache_assoc = static_cast<unsigned>(parse_u64(argv[++i])); } else { std::cerr << "--cache-assoc requires a number\n"; return 1; }
    } else if (a == "--cache-write-back") {
      cache_write_back = true;
    } else if (a == "--cache-write-through") {
      cache_write_back = false;
    } else if (a == "--cache-write-allocate") {
      cache_write_allocate = true;
    } else if (a == "--cache-no-write-allocate" || a == "--cache-write-around") {
      cache_write_allocate = false;
    } else if (a == "--step" || a == "-s") {
      step_mode = true;
    } else if (a == "--pause-on-start") {
      pause_on_start = true;
    } else if (a == "--trace-json") {
      if (i + 1 < argc) {
        trace_json_target = argv[++i];
      } else {
        std::cerr << "--trace-json requires a target (stdout or file path)\n";
        return 1;
      }
    } else if (a == "--breakpoint" || a == "-b") {
      if (i + 1 < argc) {
        breakpoints.push_back(static_cast<uint32_t>(parse_u64(argv[++i])));
      } else {
        std::cerr << "--breakpoint requires an address\n";
        return 1;
      }
    } else if (a == "--arg" || a == "-a") {
      if (i + 1 < argc) { cli_argv.push_back(argv[++i]); } else { std::cerr << "--arg requires a value\n"; return 1; }
    } else if (a == "--env" || a == "-e") {
      if (i + 1 < argc) { cli_envp.push_back(argv[++i]); } else { std::cerr << "--env requires a key=value\n"; return 1; }
    } else if (a == "--uart-ier") {
      if (i + 1 < argc) { uart_ier_val = static_cast<uint32_t>(parse_u64(argv[++i])); } else { std::cerr << "--uart-ier requires a value\n"; return 1; }
    } else if (a == "--cycles" || a == "-c") {
      if (i + 1 < argc) { run_cycles = parse_u64(argv[++i]); run_cycles_set = true; } else { std::cerr << "--cycles requires a number\n"; return 1; }
    } else if (a == "--max-cycles") {
      if (i + 1 < argc) { run_cycles = parse_u64(argv[++i]); run_cycles_set = true; } else { std::cerr << "--max-cycles requires a number\n"; return 1; }
    } else if (a == "--help" || a == "-h") {
      std::cout << "Usage: mycpu [--uart-stdin|-u] [--load <file>|-l <file>] [--mtvec <addr>|-m <addr>] [--timer-interval <n>] [--timer-mtimecmp <v>] [--timer-enable] [--uart-ier <v>] [--cycles|-c <n>] [--max-cycles <n>] [--trace-json <stdout|file>] [--breakpoint|-b <addr>] [--pause-on-start] [--verbose|-v] [--step|-s] [--arg|-a <arg>] [--env|-e <key=value>] [--no-cache] [--cache-penalty <n>] [--cache-size <bytes>] [--cache-line-size <bytes>] [--cache-assoc <n>] [--cache-write-back|--cache-write-through] [--cache-write-allocate|--cache-no-write-allocate]" << std::endl;
      return 0;
    }
  }

  if (cache_size == 0) {
    std::cerr << "--cache-size must be > 0\n";
    return 1;
  }
  if (cache_line_size == 0 || (cache_line_size & 3u) != 0u) {
    std::cerr << "--cache-line-size must be a non-zero multiple of 4\n";
    return 1;
  }
  if (cache_assoc == 0) {
    std::cerr << "--cache-assoc must be > 0\n";
    return 1;
  }

  const bool trace_to_stdout = (trace_json_target == "stdout" || trace_json_target == "-");
  std::ostream& ui_out = trace_to_stdout ? std::cerr : std::cout;

  if (!quiet) ui_out << "myCPU simulator starting" << (enable_uart_bridge ? " with UART stdin bridge" : "") << "\n";

  // If quiet mode requested, silence stderr to avoid debug/driver noise
  if (quiet) {
    static std::ofstream devnull("/dev/null");
    std::cerr.rdbuf(devnull.rdbuf());
  }

  std::signal(SIGINT, sigint_handler);

  // Allocate pipeline memory to match benchmark linker script (32MB)
  cpu::Pipeline p({}, 32 * 1024 * 1024);
  p.set_verbose(!quiet);
  // configure cache according to CLI flags
  {
    CacheConfig cfg;
    cfg.cache_size = cache_size;
    cfg.line_size = cache_line_size;
    cfg.associativity = cache_assoc;
    cfg.write_back = cache_write_back;
    cfg.write_allocate = cache_write_allocate;
    cfg.miss_latency = cache_penalty;
    p.configure_cache(!no_cache, cfg);
    p.set_uncached_latency(no_cache ? cache_penalty : 0);
    if (!quiet) {
      ui_out << "cache: " << (!no_cache ? "on" : "off")
             << " size=" << cfg.cache_size
             << " line=" << cfg.line_size
             << " assoc=" << cfg.associativity
             << " wb=" << (cfg.write_back ? 1 : 0)
             << " wa=" << (cfg.write_allocate ? 1 : 0)
             << " miss_latency=" << cfg.miss_latency << "\n";
    }
  }

  if (!trace_json_target.empty()) {
    std::string trace_error;
    if (!p.enable_trace_json(trace_json_target, &trace_error)) {
      std::cerr << (trace_error.empty() ? "failed to enable trace output" : trace_error) << "\n";
      return 1;
    }
    if (!quiet) {
      ui_out << "trace-json enabled: " << trace_json_target << "\n";
    }
  }

  if (!elf_path.empty()) {
    if (!p.load_elf(elf_path, cli_argv, cli_envp)) {
      std::cerr << "failed to load ELF: " << elf_path << "\n";
      return 1;
    }
    if (!quiet) ui_out << "loaded ELF: " << elf_path << " entry PC=" << p.pc() << "\n";
  }

  // apply CLI-configured CSRs/devices
  if (mtvec_set) p.csr().write_mtvec(static_cast<uint32_t>(mtvec_val));
  if (timer_interval > 0) p.timer().set_interval(static_cast<uint32_t>(timer_interval));
  if (timer_mtimecmp_set) p.timer().set_mtimecmp(timer_mtimecmp);
  if (timer_enable) p.timer().set_enabled(true);
  if (uart_ier_val) p.uart_write_ier(uart_ier_val);

  if (enable_uart_bridge) p.start_uart_stdin_bridge();

  // log CLI-provided argv/envp to runtime log
  if (!quiet) {
    if (cli_argv.empty() && cli_envp.empty()) {
      ui_out << "runtime: no args/env\n";
    } else {
      if (!cli_argv.empty()) {
        ui_out << "runtime argv=[";
        for (size_t i = 0; i < cli_argv.size(); ++i) {
          if (i) ui_out << ", ";
          ui_out << '"' << cli_argv[i] << '"';
        }
        ui_out << "]";
      }
      if (!cli_envp.empty()) {
        if (!cli_argv.empty()) ui_out << ' ';
        ui_out << "runtime env=[";
        for (size_t i = 0; i < cli_envp.size(); ++i) {
          if (i) ui_out << ", ";
          ui_out << '"' << cli_envp[i] << '"';
        }
        ui_out << "]";
      }
      ui_out << "\n";
    }
  }

  // main loop: step CPU until interrupted or cycles exhausted
  // helper: read a line from /dev/tty if available to avoid consuming STDIN used by UART bridge
  auto read_tty_line = [&]()->std::string {
    std::ifstream tty("/dev/tty");
    if (!tty.is_open()) {
      std::string s;
      std::getline(std::cin, s);
      return s;
    }
    std::string s;
    std::getline(tty, s);
    return s;
  };

  auto report_crash = [&p, &ui_out](const std::exception& ex) {
    ui_out << "FATAL: " << ex.what() << "\n";
    ui_out << "Crash OOB: access=" << (p.memory().last_oob_is_write() ? "write" : "read")
           << " size=" << p.memory().last_oob_size()
           << " addr=0x" << std::hex << p.memory().last_oob_addr() << std::dec << "\n";
    ui_out << "Crash Regs: "
           << "sp=0x" << std::hex << p.regs().read(2)
           << " s0=0x" << p.regs().read(8)
           << " s1=0x" << p.regs().read(9)
           << " a0=0x" << p.regs().read(10)
           << " a1=0x" << p.regs().read(11)
           << " a2=0x" << p.regs().read(12)
           << " a3=0x" << p.regs().read(13)
           << " a4=0x" << p.regs().read(14)
           << " a5=0x" << p.regs().read(15)
           << std::dec << "\n";

    if (p.exmem().valid) {
      ui_out << "Crash Stage: MEM pc=0x" << std::hex << p.exmem().pc
             << " inst=" << p.exmem().inst.name
             << " alu=0x" << p.exmem().alu_result << std::dec << "\n";
    } else if (p.idex().valid) {
      ui_out << "Crash Stage: EX pc=0x" << std::hex << p.idex().pc
             << " inst=" << p.idex().inst.name << std::dec << "\n";
    } else if (p.ifid().valid) {
      ui_out << "Crash Stage: ID pc=0x" << std::hex << p.ifid().pc
             << " inst=" << p.ifid().inst.name << std::dec << "\n";
    }

    uint32_t cur_pc = p.pc();
    ui_out << "Crash PC: 0x" << std::hex << cur_pc;
    if (cur_pc + 4 <= static_cast<uint32_t>(p.memory().size())) {
      try {
        uint32_t word = p.memory().load32(cur_pc);
        auto d = isa::decode(word);
        ui_out << " instr_word=0x" << word << " instr=" << d.name;
      } catch (...) {
        // ignore secondary read failure while reporting crash context
      }
    }
    ui_out << std::dec << "\n";
  };

  auto step_once = [&]() -> bool {
    try {
      p.step();
      return true;
    } catch (const std::out_of_range& ex) {
      report_crash(ex);
      return false;
    } catch (const std::exception& ex) {
      report_crash(ex);
      return false;
    }
  };

  auto hit_breakpoint = [&]() -> bool {
    for (uint32_t bp : breakpoints) {
      if (p.pc() == bp) {
        ui_out << "breakpoint hit at pc=0x" << std::hex << p.pc() << std::dec << "\n";
        return true;
      }
    }
    return false;
  };

  if (pause_on_start && running.load()) {
    ui_out << "(pause-on-start) press Enter to start, 'q' to quit\n";
    std::string l = read_tty_line();
    if (!l.empty() && (l[0] == 'q' || l[0] == 'Q')) {
      running.store(false);
    }
  }

  if (run_cycles_set) {
    for (uint64_t i = 0; i < run_cycles && running.load(); ++i) {
      if (hit_breakpoint()) { running.store(false); break; }
      if (!step_once()) return 2;
      if (periph::tohost_exit_code.load() >= 0) { running.store(false); break; }
      if (verbose) ui_out << p.dump_regs() << "\n";
      if (step_mode) {
        ui_out << "(step) press Enter to continue, 'q' to quit\n";
        std::string l = read_tty_line();
        if (!l.empty() && (l[0] == 'q' || l[0] == 'Q')) { running.store(false); break; }
      }
    }
  } else {
    while (running.load()) {
      if (hit_breakpoint()) { running.store(false); break; }
      if (!step_once()) return 2;
      if (periph::tohost_exit_code.load() >= 0) { running.store(false); break; }
      if (verbose) ui_out << p.dump_regs() << "\n";
      if (step_mode) {
        ui_out << "(step) press Enter to continue, 'q' to quit\n";
        std::string l = read_tty_line();
        if (!l.empty() && (l[0] == 'q' || l[0] == 'Q')) { running.store(false); break; }
      } else {
        // don't throttle simulator in quiet/benchmark mode
        if (!quiet) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  // If semihosting wrote an exit code, use it as process exit code
  int tohost_rc = periph::tohost_exit_code.load();

  // gather stats
  uint64_t cycles = p.cycles();
  uint64_t instrs = p.instrs();
  double i_acc = static_cast<double>(p.icache_accesses());
  double i_hits = static_cast<double>(p.icache_hits());
  double i_pct = (i_acc > 0.0) ? (i_hits / i_acc * 100.0) : 0.0;
  uint64_t i_miss = p.icache_misses();
  uint64_t i_evict = p.icache_evictions();
  uint64_t i_wb = p.icache_writebacks();
  uint64_t i_cold = p.icache_cold_misses();
  uint64_t i_conflict = p.icache_conflict_misses();
  uint64_t i_capacity = p.icache_capacity_misses();
  double d_acc = static_cast<double>(p.dcache_accesses());
  double d_hits = static_cast<double>(p.dcache_hits());
  double d_pct = (d_acc > 0.0) ? (d_hits / d_acc * 100.0) : 0.0;
  uint64_t d_miss = p.dcache_misses();
  uint64_t d_evict = p.dcache_evictions();
  uint64_t d_wb = p.dcache_writebacks();
  uint64_t d_cold = p.dcache_cold_misses();
  uint64_t d_conflict = p.dcache_conflict_misses();
  uint64_t d_capacity = p.dcache_capacity_misses();
  uint64_t stall_all = p.stall_cycles();
  uint64_t stall_cache = p.cache_stall_cycles();
  uint64_t stall_hazard = p.hazard_stall_cycles();

  // final single-line performance summary (always printed)
    ui_out << "Cycles: " << cycles << ", Instrs: " << instrs
      << ", I-Cache Hit: " << std::fixed << std::setprecision(2) << i_pct << "%"
      << ", D-Cache Hit: " << std::fixed << std::setprecision(2) << d_pct << "%"
      << ", I-Miss: " << i_miss
      << ", D-Miss: " << d_miss
      << ", I-Evict: " << i_evict
      << ", D-Evict: " << d_evict
      << ", I-WB: " << i_wb
      << ", D-WB: " << d_wb
      << ", I-ColdMiss: " << i_cold
      << ", I-ConflictMiss: " << i_conflict
      << ", I-CapacityMiss: " << i_capacity
      << ", D-ColdMiss: " << d_cold
      << ", D-ConflictMiss: " << d_conflict
      << ", D-CapacityMiss: " << d_capacity
      << ", Stall: " << stall_all
      << ", CacheStall: " << stall_cache
      << ", HazardStall: " << stall_hazard
      << std::endl;

    if (!quiet) ui_out << "myCPU simulator exiting" << std::endl;

  return (tohost_rc >= 0) ? tohost_rc : 0;
}
