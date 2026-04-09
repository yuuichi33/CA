#include "trace/trace.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace trace {

namespace {

std::string JsonEscape(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 8);
  for (char c : input) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

void AppendStage(std::ostringstream& oss, const char* key, const StageState& stage) {
  oss << "\"" << key << "\":{"
      << "\"valid\":" << (stage.valid ? "true" : "false")
      << ",\"pc\":" << stage.pc
      << ",\"instr\":\"" << JsonEscape(stage.instr) << "\""
      << ",\"rd\":" << stage.rd
      << ",\"write_back\":" << (stage.write_back ? "true" : "false")
      << ",\"alu_result\":" << stage.alu_result
      << ",\"mem_data\":" << stage.mem_data
      << "}";
}

}  // namespace

std::unique_ptr<TraceWriter> TraceWriter::Create(const std::string& target, std::string* error) {
  auto writer = std::unique_ptr<TraceWriter>(new TraceWriter());

  if (target == "stdout" || target == "-") {
    writer->out_ = &std::cout;
    return writer;
  }

  auto file_stream = std::make_unique<std::ofstream>(target, std::ios::out | std::ios::trunc);
  if (!file_stream->is_open()) {
    if (error) {
      *error = "failed to open trace output: " + target;
    }
    return nullptr;
  }

  writer->out_ = file_stream.get();
  writer->owned_out_ = std::move(file_stream);
  return writer;
}

void TraceWriter::WriteCycle(const CycleRecord& record) {
  if (!out_) {
    return;
  }

  std::ostringstream oss;
  oss << "{"
      << "\"cycle\":" << record.cycle
      << ",\"pc\":" << record.pc
      << ",\"instrs_retired\":" << record.instrs_retired
      << ",\"stalled\":" << (record.stalled ? "true" : "false")
      << ",\"tohost\":" << record.tohost
      << ",\"exception\":\"" << JsonEscape(record.exception) << "\""
      << ",\"metrics\":{"
      << "\"icache_accesses\":" << record.icache_accesses
      << ",\"icache_hits\":" << record.icache_hits
      << ",\"icache_misses\":" << record.icache_misses
      << ",\"icache_evictions\":" << record.icache_evictions
      << ",\"icache_writebacks\":" << record.icache_writebacks
      << ",\"dcache_accesses\":" << record.dcache_accesses
      << ",\"dcache_hits\":" << record.dcache_hits
      << ",\"dcache_misses\":" << record.dcache_misses
      << ",\"dcache_evictions\":" << record.dcache_evictions
      << ",\"dcache_writebacks\":" << record.dcache_writebacks
      << ",\"stall_cycles\":" << record.stall_cycles
      << ",\"cache_stall_cycles\":" << record.cache_stall_cycles
      << ",\"hazard_stall_cycles\":" << record.hazard_stall_cycles
      << "},\"stages\":{";

  AppendStage(oss, "if", record.if_stage);
  oss << ",";
  AppendStage(oss, "id", record.id_stage);
  oss << ",";
  AppendStage(oss, "ex", record.ex_stage);
  oss << ",";
  AppendStage(oss, "mem", record.mem_stage);
  oss << ",";
  AppendStage(oss, "wb", record.wb_stage);
  oss << "},\"regs_changed\":[";

  for (size_t i = 0; i < record.regs_changed.size(); ++i) {
    const auto& rw = record.regs_changed[i];
    if (i) {
      oss << ",";
    }
    oss << "{\"rd\":" << rw.rd << ",\"value\":" << rw.value << "}";
  }

  oss << "],\"mem_accesses\":[";
  for (size_t i = 0; i < record.mem_accesses.size(); ++i) {
    const auto& ma = record.mem_accesses[i];
    if (i) {
      oss << ",";
    }
    oss << "{\"stage\":\"" << JsonEscape(ma.stage) << "\""
        << ",\"type\":\"" << JsonEscape(ma.type) << "\""
        << ",\"vaddr\":" << ma.vaddr
        << ",\"paddr\":" << ma.paddr
        << ",\"size\":" << ma.size
        << ",\"value\":" << ma.value
        << ",\"mmio\":" << (ma.mmio ? "true" : "false")
        << ",\"cache_used\":" << (ma.cache_used ? "true" : "false")
        << ",\"cache_hit\":" << (ma.cache_hit ? "true" : "false")
        << "}";
  }

  oss << "],\"cache_events\":[";
  for (size_t i = 0; i < record.cache_events.size(); ++i) {
    const auto& ce = record.cache_events[i];
    if (i) {
      oss << ",";
    }
    oss << "{\"cache\":\"" << JsonEscape(ce.cache) << "\""
        << ",\"op\":\"" << JsonEscape(ce.op) << "\""
        << ",\"paddr\":" << ce.paddr
        << ",\"size\":" << ce.size
        << ",\"hit\":" << (ce.hit ? "true" : "false")
        << ",\"latency\":" << ce.latency
        << "}";
  }

  oss << "]}";

  (*out_) << oss.str() << '\n';
  out_->flush();
}

}  // namespace trace
