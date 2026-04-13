# myCPU

myCPU is a C++17 RISC-V simulator for teaching and micro-architecture experiments.
It implements an RV32I 5-stage pipeline with privilege support, Sv32 MMU,
I/D cache, MMIO peripherals, ELF loading, and trace visualization.

## Features

- Pipeline: IF / ID / EX / MEM / WB
- ISA: RV32I + CSR/system instructions (ECALL/MRET/SRET/SFENCE.VMA/FENCE/FENCE.I)
- Privilege: M/S/U modes with trap and delegation paths
- MMU: Sv32 page walk + ASID-aware TLB
- Cache: split I-cache/D-cache, configurable write/allocate policies
- Stats: cycles/instrs, I/D hit rate, stall/cache_stall/hazard_stall
- Trace: JSONL + SSE middleware + web dashboard

## Instruction Coverage

- Effective instruction count: 49 (excluding placeholder labels UNKNOWN/SYSTEM)

| Category | Count | Instructions |
| --- | ---: | --- |
| R-type arithmetic | 10 | ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND |
| I-type arithmetic | 9 | ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI |
| Load | 5 | LB, LH, LW, LBU, LHU |
| Store | 3 | SB, SH, SW |
| Branch | 6 | BEQ, BNE, BLT, BGE, BLTU, BGEU |
| U-type | 2 | LUI, AUIPC |
| JAL | 1 | JAL |
| JALR | 1 | JALR |
| CSR/privileged | 10 | ECALL, MRET, SRET, SFENCE.VMA, CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI |
| Fence | 2 | FENCE, FENCE.I |

- Baseline ISA scope: RV32I + CSR/system instructions used by the current privilege/MMU flow.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Full Test Matrix

### Data semantics

- `p1`: cache enabled, `--cache-penalty 1`
- `p10`: cache enabled, `--cache-penalty 10`
- `no-cache`: cache disabled with `--no-cache --cache-penalty 10`

### Run steps

```bash
# 1) ctest
ctest --test-dir build --output-on-failure

# 2) rv32ui (42 tests x 3 profiles)
bash test_all.sh --csv docs/rv32ui_perf_full_p1.csv --cache-penalty 1 --quiet
bash test_all.sh --csv docs/rv32ui_perf_full_p10.csv --cache-penalty 10 --quiet
bash test_all.sh --csv docs/rv32ui_perf_full_nocache.csv --no-cache --cache-penalty 10 --quiet

# 3) benchmark sample runs
./build/mycpu --quiet --load benchmarks/hello.elf --cache-penalty 10
./build/mycpu --quiet --load benchmarks/matmul.elf --cache-penalty 10
./build/mycpu --quiet --load benchmarks/matmul.elf --no-cache --cache-penalty 10
./build/mycpu --quiet --load benchmarks/quicksort_stress.elf --cache-penalty 10
./build/mycpu --quiet --load benchmarks/quicksort_stress.elf --no-cache --cache-penalty 10

# 4) generate report + charts
python3 tools/gen_full_test_report.py --run-tag 20260409
```

### Outputs

- `docs/FULL_TEST_PERFORMANCE_REPORT.md`
- `docs/full_test_summary_<run-tag>.csv`
- `docs/figures/full_run_<run-tag>_speedup_bar.png`
- `docs/figures/full_run_<run-tag>_hitrate_scatter.png`
- `docs/figures/full_run_<run-tag>_benchmark_cycles_log.png`

## Cache Highlights

- Split I-cache / D-cache with configurable size, line size, associativity and miss penalty.
- Configurable policies: write-back/write-through and write-allocate/no-write-allocate.
- Verified correctness: rv32ui p1/p10/no-cache are all 42/42 PASS.
- Quantified gains (full report):
  - rv32ui p10 average speedup: 6.47x
  - p10/p1 average cycle ratio: 1.54x
  - matmul no-cache/cache: 11.34x
  - quicksort no-cache/cache: 12.71x

## Trace Dashboard

```bash
chmod +x tools/run_trace_demo.sh
./tools/run_trace_demo.sh

# optional: custom ELF and max cycles
./tools/run_trace_demo.sh benchmarks/hello.elf 200

# optional: force a fixed port
TRACE_PORT=8080 ./tools/run_trace_demo.sh benchmarks/hello.elf 200
```

- The launcher prints the final URL to open in browser.
- If port 8080 is occupied, the launcher auto-falls back (for example 18080/18081).
- The page auto-connects to SSE on load; Connect SSE is still available for manual reconnect.
- Health check should use the printed port, for example: `curl -s http://127.0.0.1:18080/health`
- See also: `web/README.md`

## Repository Layout

```text
camycpu/
├── benchmarks/                # hello/matmul/quicksort_stress + linker/startup
├── docs/                      # current deliverables: reports, full csv, figures
├── archive/                   # stage archives and historical docs/csv
├── riscv-tests/               # git submodule (official tests)
├── src/                       # simulator source
├── tests/                     # ctest targets (19 files)
├── tools/                     # report generator + trace server scripts
├── web/                       # dashboard frontend
├── tmp/                       # temporary logs and intermediate files
├── build/                     # rebuildable artifacts
├── Testing/                   # rebuildable ctest artifacts
├── CMakeLists.txt
├── README.md
└── test_all.sh
```

## Key Files

- `tools/gen_full_test_report.py`: generates markdown report, summary CSV, and PNG charts
- `docs/FULL_TEST_PERFORMANCE_REPORT.md`: consolidated full-run report
- `docs/rv32ui_perf_full_p1.csv`: rv32ui profile p1 dataset
- `docs/rv32ui_perf_full_p10.csv`: rv32ui profile p10 dataset
- `docs/rv32ui_perf_full_nocache.csv`: rv32ui profile no-cache dataset
- `docs/DOCS_CATALOG.md`: docs md/csv classification and purpose index
- `web/index.html`, `web/app.js`, `web/style.css`: trace dashboard

## Docs Organization

- `docs/` keeps current delivery artifacts only:
  - `FULL_TEST_PERFORMANCE_REPORT.md`
  - `BENCHMARK_REPORT.md`
  - `full_test_summary_*.csv`
  - `rv32ui_perf_full_*.csv`
  - `figures/`
- `archive/docs-history/reports/` keeps historical markdown reports.
- `archive/docs-history/csv-legacy/` keeps historical csv datasets.
- See `docs/DOCS_CATALOG.md` for file-by-file explanations.

## Archive And Cleanup

- Archive stage outputs under `archive/` with stage folders (`stage-01` ... `stage-05`) and `MANIFEST.md`
- Safe to delete and rebuild:
  - `build/`
  - `Testing/`
  - `tmp/full_run_*/` (after final docs artifacts are confirmed)
- Keep final docs artifacts in `docs/`
