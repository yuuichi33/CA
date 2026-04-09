const d3 = await import('https://cdn.jsdelivr.net/npm/d3@7/+esm');

const STAGES = [
  { key: 'if', label: 'IF' },
  { key: 'id', label: 'ID' },
  { key: 'ex', label: 'EX' },
  { key: 'mem', label: 'MEM' },
  { key: 'wb', label: 'WB' }
];

const UART_MMIO_TX_ADDR = 0x10001000;

const dom = {
  connectBtn: document.getElementById('connectBtn'),
  disconnectBtn: document.getElementById('disconnectBtn'),
  clearBtn: document.getElementById('clearBtn'),
  playPauseBtn: document.getElementById('playPauseBtn'),
  stepBackBtn: document.getElementById('stepBackBtn'),
  stepForwardBtn: document.getElementById('stepForwardBtn'),
  clearConsoleBtn: document.getElementById('clearConsoleBtn'),
  speedRange: document.getElementById('speedRange'),
  speedLabel: document.getElementById('speedLabel'),
  jumpCycleInput: document.getElementById('jumpCycleInput'),
  jumpCycleBtn: document.getElementById('jumpCycleBtn'),
  followTailCheck: document.getElementById('followTailCheck'),
  connState: document.getElementById('connState'),
  lineCount: document.getElementById('lineCount'),
  lastCycle: document.getElementById('lastCycle'),
  lastPc: document.getElementById('lastPc'),
  lastEx: document.getElementById('lastEx'),
  iRateBar: document.getElementById('iRateBar'),
  dRateBar: document.getElementById('dRateBar'),
  iRateText: document.getElementById('iRateText'),
  dRateText: document.getElementById('dRateText'),
  cacheEventList: document.getElementById('cacheEventList'),
  memEventList: document.getElementById('memEventList'),
  memBaseInput: document.getElementById('memBaseInput'),
  memSizeInput: document.getElementById('memSizeInput'),
  memApplyBtn: document.getElementById('memApplyBtn'),
  memAtPcBtn: document.getElementById('memAtPcBtn'),
  memLastWriteBtn: document.getElementById('memLastWriteBtn'),
  memIncludeMmioCheck: document.getElementById('memIncludeMmioCheck'),
  memFollowLastWriteCheck: document.getElementById('memFollowLastWriteCheck'),
  memRangeLabel: document.getElementById('memRangeLabel'),
  memDump: document.getElementById('memDump'),
  memWriteList: document.getElementById('memWriteList'),
  regsGrid: document.getElementById('regsGrid'),
  virtualConsole: document.getElementById('virtualConsole'),
  log: document.getElementById('log'),
  perfSvg: d3.select('#perfSvg'),
  pipelineSvg: d3.select('#pipelineSvg'),
  timelineSvg: d3.select('#timelineSvg')
};

const state = {
  source: null,
  connected: false,
  traces: [],
  rawLines: [],
  cacheEvents: [],
  memWrites: [],
  cursor: -1,
  playing: true,
  followTail: true,
  speed: 1,
  memWindowBase: 0,
  memWindowSize: 64,
  includeMmioStores: false,
  followLatestStore: false,
  virtualConsoleText: '',
  lastTick: performance.now(),
  maxVirtualConsoleChars: 20000,
  perfWindowCycles: 1000,
  maxRawLines: 160,
  maxCacheEvents: 120
};

function formatHex(value, width = 8) {
  if (typeof value !== 'number' || Number.isNaN(value)) return '-';
  return `0x${(value >>> 0).toString(16).padStart(width, '0')}`;
}

function formatPct(ratio) {
  return `${(ratio * 100).toFixed(2)}%`;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function parseAddr(text, fallback = 0) {
  if (typeof text !== 'string') return fallback >>> 0;
  const t = text.trim().toLowerCase();
  if (!t) return fallback >>> 0;
  const parsed = t.startsWith('0x') ? Number.parseInt(t.slice(2), 16) : Number.parseInt(t, 10);
  if (!Number.isFinite(parsed) || Number.isNaN(parsed)) return fallback >>> 0;
  return (parsed >>> 0);
}

function byteToAscii(b) {
  if (b >= 32 && b <= 126) return String.fromCharCode(b);
  return '.';
}

function writeBytesLE(value, size) {
  const bytes = [];
  for (let i = 0; i < size; i += 1) {
    bytes.push((value >>> (i * 8)) & 0xff);
  }
  return bytes;
}

function storeTypeToSize(type) {
  const t = String(type || '').toUpperCase();
  if (t === 'SB') return 1;
  if (t === 'SH') return 2;
  if (t === 'SW') return 4;
  return 0;
}

function decodeUartByte(byte) {
  const b = byte & 0xff;
  if (b === 10) return '\n';
  if (b === 13) return '\n';
  if (b === 9) return '\t';
  if (b === 92) return '\\';
  if (b >= 32 && b <= 126) return String.fromCharCode(b);
  return `\\x${b.toString(16).padStart(2, '0')}`;
}

function extractUartOutput(frame) {
  const memItems = Array.isArray(frame?.mem_accesses) ? frame.mem_accesses : [];
  let out = '';
  for (const ev of memItems) {
    const size = storeTypeToSize(ev.type);
    if (size === 0) continue;
    if (!ev.mmio) continue;
    if (typeof ev.paddr !== 'number' || typeof ev.value !== 'number') continue;
    const paddr = ev.paddr >>> 0;
    if (paddr < UART_MMIO_TX_ADDR || paddr >= (UART_MMIO_TX_ADDR + 4)) continue;
    out += decodeUartByte(ev.value);
  }
  return out;
}

function appendVirtualConsole(text) {
  if (!text) return;
  state.virtualConsoleText += text;
  if (state.virtualConsoleText.length > state.maxVirtualConsoleChars) {
    state.virtualConsoleText = state.virtualConsoleText.slice(
      state.virtualConsoleText.length - state.maxVirtualConsoleChars
    );
  }
}

function stageColor(stage, stalled) {
  if (!stage || !stage.valid) return '#f2eee5';
  if (stalled) return '#f6d7c8';
  return '#d9f2eb';
}

function instrPalette(instr) {
  const scheme = ['#2f936f', '#cf6b34', '#4b7ca8', '#7b5ea4', '#b86a86', '#5e9c7a', '#8f6f3f', '#5e6475'];
  let sum = 0;
  for (let i = 0; i < instr.length; i += 1) sum += instr.charCodeAt(i);
  return scheme[sum % scheme.length];
}

function emptyRegisters() {
  const regs = new Array(32).fill(0);
  return regs;
}

function ensureCursor() {
  if (state.traces.length === 0) {
    state.cursor = -1;
    return;
  }
  if (state.cursor < 0) state.cursor = 0;
  if (state.cursor >= state.traces.length) state.cursor = state.traces.length - 1;
}

function clearData() {
  state.rawLines = [];
  state.traces = [];
  state.cacheEvents = [];
  state.memWrites = [];
  state.virtualConsoleText = '';
  state.cursor = -1;
  renderAll();
}

function clearFrameDataOnly() {
  state.traces = [];
  state.cacheEvents = [];
  state.memWrites = [];
  state.virtualConsoleText = '';
  state.cursor = -1;
}

function handleTraceLine(line) {
  if (!line) return;

  state.rawLines.push(line);
  if (state.rawLines.length > state.maxRawLines) {
    state.rawLines.splice(0, state.rawLines.length - state.maxRawLines);
  }

  let obj = null;
  try {
    obj = JSON.parse(line);
  } catch (_) {
    // UART output may be prefixed to a JSON line; attempt to parse from first '{'.
    const jsonStart = line.indexOf('{');
    if (jsonStart >= 0) {
      try {
        obj = JSON.parse(line.slice(jsonStart));
      } catch (_) {
        renderLog();
        return;
      }
    } else {
      renderLog();
      return;
    }
  }

  if (obj.kind === 'meta') {
    renderLog();
    return;
  }

  if (typeof obj.cycle !== 'number') {
    renderLog();
    return;
  }

  if (state.traces.length > 0 && obj.cycle <= state.traces[state.traces.length - 1].cycle) {
    clearFrameDataOnly();
  }

  appendVirtualConsole(extractUartOutput(obj));

  const prevRegs = state.traces.length > 0 ? state.traces[state.traces.length - 1]._regs : emptyRegisters();
  const regs = prevRegs.slice();
  const changedSet = new Set();

  const changes = Array.isArray(obj.regs_changed) ? obj.regs_changed : [];
  for (const item of changes) {
    if (!item || typeof item.rd !== 'number' || typeof item.value !== 'number') continue;
    if (item.rd === 0) continue;
    regs[item.rd] = item.value >>> 0;
    changedSet.add(item.rd);
  }
  regs[0] = 0;

  obj._regs = regs;
  obj._changedSet = changedSet;

  state.traces.push(obj);

  const frameCacheEvents = Array.isArray(obj.cache_events) ? obj.cache_events : [];
  for (const ev of frameCacheEvents) {
    state.cacheEvents.push({
      cycle: obj.cycle,
      cache: ev.cache || '?',
      op: ev.op || '?',
      paddr: ev.paddr,
      size: ev.size,
      hit: !!ev.hit,
      latency: ev.latency
    });
  }
  if (state.cacheEvents.length > state.maxCacheEvents) {
    state.cacheEvents.splice(0, state.cacheEvents.length - state.maxCacheEvents);
  }

  const memAccesses = Array.isArray(obj.mem_accesses) ? obj.mem_accesses : [];
  for (const ev of memAccesses) {
    const size = storeTypeToSize(ev.type);
    if (size === 0) continue;
    if (typeof ev.paddr !== 'number' || typeof ev.value !== 'number') continue;
    state.memWrites.push({
      cycle: obj.cycle,
      paddr: ev.paddr >>> 0,
      size,
      type: String(ev.type || '').toUpperCase(),
      mmio: !!ev.mmio,
      bytes: writeBytesLE(ev.value >>> 0, size)
    });
  }

  ensureCursor();
  if (state.followTail) {
    state.cursor = state.traces.length - 1;
  }
  renderAll();
}

function setConnected(connected) {
  state.connected = connected;
  dom.connState.textContent = connected ? 'connected' : 'disconnected';
  dom.connectBtn.disabled = connected;
  dom.disconnectBtn.disabled = !connected;
}

function connectSSE() {
  if (state.source) return;
  const source = new EventSource('/events');
  state.source = source;

  source.onopen = () => {
    setConnected(true);
  };

  source.onmessage = (event) => {
    handleTraceLine(event.data);
  };

  source.onerror = () => {
    if (!state.source) return;
    if (state.source.readyState === EventSource.CLOSED) {
      state.source = null;
      setConnected(false);
    } else {
      dom.connState.textContent = 'reconnecting';
    }
  };
}

function disconnectSSE() {
  if (!state.source) return;
  state.source.close();
  state.source = null;
  setConnected(false);
}

function currentFrame() {
  if (state.cursor < 0 || state.cursor >= state.traces.length) return null;
  return state.traces[state.cursor];
}

function renderStatus(frame) {
  dom.lineCount.textContent = String(state.traces.length);

  if (!frame) {
    dom.lastCycle.textContent = '-';
    dom.lastPc.textContent = '-';
    dom.lastEx.textContent = '-';
    return;
  }

  dom.lastCycle.textContent = String(frame.cycle ?? '-');
  dom.lastPc.textContent = formatHex(frame.pc);
  dom.lastEx.textContent = frame.exception && frame.exception.length > 0 ? frame.exception : '-';
}

function renderPipeline(frame) {
  const svg = dom.pipelineSvg;
  const node = svg.node();
  const width = Math.max(640, node.clientWidth || 640);
  const height = 180;
  svg.attr('viewBox', `0 0 ${width} ${height}`);
  svg.selectAll('*').remove();

  if (!frame) {
    svg.append('text')
      .attr('x', 20)
      .attr('y', 36)
      .attr('fill', '#696f68')
      .text('等待 trace 数据...');
    return;
  }

  const stages = frame.stages || {};
  const x = d3.scaleBand()
    .domain(STAGES.map((s) => s.key))
    .range([20, width - 20])
    .padding(0.1);

  const g = svg.append('g');

  const stageG = g.selectAll('g.stage')
    .data(STAGES)
    .enter()
    .append('g')
    .attr('class', 'stage')
    .attr('transform', (d) => `translate(${x(d.key)}, 24)`);

  stageG.append('rect')
    .attr('width', x.bandwidth())
    .attr('height', 118)
    .attr('rx', 12)
    .attr('fill', (d) => stageColor(stages[d.key], frame.stalled))
    .attr('stroke', '#cebfa7');

  stageG.append('text')
    .attr('x', 12)
    .attr('y', 22)
    .attr('font-family', 'IBM Plex Mono, monospace')
    .attr('font-size', 13)
    .attr('font-weight', 700)
    .attr('fill', '#2f3933')
    .text((d) => d.label);

  stageG.append('text')
    .attr('x', 12)
    .attr('y', 50)
    .attr('font-family', 'IBM Plex Mono, monospace')
    .attr('font-size', 12)
    .attr('fill', '#2d2922')
    .text((d) => {
      const st = stages[d.key] || {};
      return st.valid ? (st.instr || 'UNKNOWN') : 'bubble';
    });

  stageG.append('text')
    .attr('x', 12)
    .attr('y', 72)
    .attr('font-family', 'IBM Plex Mono, monospace')
    .attr('font-size', 11)
    .attr('fill', '#565246')
    .text((d) => {
      const st = stages[d.key] || {};
      return `pc ${formatHex(st.pc || 0)}`;
    });

  stageG.append('text')
    .attr('x', 12)
    .attr('y', 92)
    .attr('font-family', 'IBM Plex Mono, monospace')
    .attr('font-size', 10)
    .attr('fill', '#666')
    .text((d) => {
      const st = stages[d.key] || {};
      return st.valid && st.write_back ? `rd x${st.rd}` : '';
    });

  if (frame.stalled) {
    svg.append('text')
      .attr('x', width - 170)
      .attr('y', 18)
      .attr('fill', '#b23838')
      .attr('font-family', 'IBM Plex Mono, monospace')
      .attr('font-size', 12)
      .attr('font-weight', 700)
      .text('STALL CYCLE');
  }

  if (frame.exception) {
    svg.append('text')
      .attr('x', 20)
      .attr('y', 18)
      .attr('fill', '#b23838')
      .attr('font-family', 'IBM Plex Mono, monospace')
      .attr('font-size', 12)
      .text(`exception: ${frame.exception}`);
  }
}

function renderTimeline(frame) {
  const svg = dom.timelineSvg;
  const node = svg.node();
  const width = Math.max(640, node.clientWidth || 640);
  const height = 240;
  svg.attr('viewBox', `0 0 ${width} ${height}`);
  svg.selectAll('*').remove();

  if (!frame || state.traces.length === 0) {
    svg.append('text')
      .attr('x', 18)
      .attr('y', 34)
      .attr('fill', '#696f68')
      .text('暂无时间轴数据');
    return;
  }

  const idx = state.cursor;
  const start = Math.max(0, idx - 31);
  const windowFrames = state.traces.slice(start, idx + 1);

  const minCycle = windowFrames[0].cycle;
  const maxCycle = windowFrames[windowFrames.length - 1].cycle;

  const margin = { top: 18, right: 20, bottom: 30, left: 60 };
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;

  const x = d3.scaleLinear()
    .domain([minCycle, Math.max(minCycle + 1, maxCycle)])
    .range([margin.left, margin.left + plotW]);

  const y = d3.scaleBand()
    .domain(STAGES.map((s) => s.key))
    .range([margin.top, margin.top + plotH])
    .padding(0.22);

  svg.append('g')
    .attr('transform', `translate(0, ${margin.top + plotH})`)
    .call(d3.axisBottom(x).ticks(7).tickFormat((v) => `${v}`))
    .call((g) => g.selectAll('text').attr('font-family', 'IBM Plex Mono, monospace').attr('font-size', 10));

  svg.append('g')
    .attr('transform', `translate(${margin.left}, 0)`)
    .call(d3.axisLeft(y).tickFormat((v) => v.toUpperCase()))
    .call((g) => g.selectAll('text').attr('font-family', 'IBM Plex Mono, monospace').attr('font-size', 11));

  const events = [];
  for (const f of windowFrames) {
    const stages = f.stages || {};
    for (const s of STAGES) {
      const st = stages[s.key] || {};
      if (st.valid) {
        events.push({
          cycle: f.cycle,
          stage: s.key,
          instr: st.instr || 'UNKNOWN',
          pc: st.pc || 0,
          stalled: !!f.stalled
        });
      }
    }
  }

  svg.append('g')
    .selectAll('circle.event')
    .data(events)
    .enter()
    .append('circle')
    .attr('cx', (d) => x(d.cycle))
    .attr('cy', (d) => (y(d.stage) || 0) + y.bandwidth() / 2)
    .attr('r', 5)
    .attr('fill', (d) => instrPalette(d.instr))
    .attr('stroke', (d) => (d.stalled ? '#b23838' : '#fdfcf9'))
    .attr('stroke-width', (d) => (d.stalled ? 1.8 : 0.9));

  const stalledFrames = windowFrames.filter((f) => f.stalled);
  svg.append('g')
    .selectAll('line.stall')
    .data(stalledFrames)
    .enter()
    .append('line')
    .attr('x1', (d) => x(d.cycle))
    .attr('x2', (d) => x(d.cycle))
    .attr('y1', margin.top)
    .attr('y2', margin.top + plotH)
    .attr('stroke', '#f3c4c4')
    .attr('stroke-dasharray', '3,3');

  svg.append('text')
    .attr('x', width - 12)
    .attr('y', 14)
    .attr('text-anchor', 'end')
    .attr('font-family', 'IBM Plex Mono, monospace')
    .attr('font-size', 11)
    .attr('fill', '#5e5a4f')
    .text(`cycles ${minCycle}..${maxCycle}`);
}

function renderRegs(frame) {
  const regs = frame ? frame._regs : emptyRegisters();
  const changedSet = frame ? frame._changedSet : new Set();

  const data = d3.range(32).map((idx) => ({
    idx,
    value: regs[idx] || 0,
    changed: changedSet.has(idx)
  }));

  const cells = d3.select(dom.regsGrid)
    .selectAll('div.reg-cell')
    .data(data, (d) => d.idx);

  const cellsEnter = cells.enter()
    .append('div')
    .attr('class', 'reg-cell');

  cellsEnter.append('span').attr('class', 'reg-name');
  cellsEnter.append('span').attr('class', 'reg-value');

  cells.merge(cellsEnter)
    .attr('class', (d) => `reg-cell${d.changed ? ' changed' : ''}`)
    .select('.reg-name')
    .text((d) => `x${d.idx}`);

  cells.merge(cellsEnter)
    .select('.reg-value')
    .text((d) => formatHex(d.value));

  cells.exit().remove();
}

function renderCache(frame) {
  if (!frame || !frame.metrics) {
    dom.iRateBar.style.width = '0%';
    dom.dRateBar.style.width = '0%';
    dom.iRateText.textContent = '0.00%';
    dom.dRateText.textContent = '0.00%';
    dom.cacheEventList.innerHTML = '<li class="placeholder">暂无 cache 事件</li>';
    dom.memEventList.innerHTML = '<li class="placeholder">暂无访存事件</li>';
    return;
  }

  const m = frame.metrics;
  const iRate = m.icache_accesses > 0 ? m.icache_hits / m.icache_accesses : 0;
  const dRate = m.dcache_accesses > 0 ? m.dcache_hits / m.dcache_accesses : 0;

  dom.iRateBar.style.width = `${(iRate * 100).toFixed(2)}%`;
  dom.dRateBar.style.width = `${(dRate * 100).toFixed(2)}%`;
  dom.iRateText.textContent = formatPct(iRate);
  dom.dRateText.textContent = formatPct(dRate);

  const cacheItems = state.cacheEvents
    .filter((ev) => ev.cycle <= frame.cycle)
    .slice(-8)
    .reverse();

  dom.cacheEventList.innerHTML = '';
  if (cacheItems.length === 0) {
    dom.cacheEventList.innerHTML = '<li class="placeholder">暂无 cache 事件</li>';
  } else {
    for (const ev of cacheItems) {
      const li = document.createElement('li');
      li.textContent = `[c${ev.cycle}] ${ev.cache.toUpperCase()} ${ev.op} ${ev.hit ? 'hit' : 'miss'} addr=${formatHex(ev.paddr)}`;
      dom.cacheEventList.appendChild(li);
    }
  }

  const memItems = Array.isArray(frame.mem_accesses) ? frame.mem_accesses : [];
  dom.memEventList.innerHTML = '';
  if (memItems.length === 0) {
    dom.memEventList.innerHTML = '<li class="placeholder">当前周期无访存</li>';
  } else {
    for (const ev of memItems) {
      const li = document.createElement('li');
      li.textContent = `${ev.stage} ${ev.type} v=${formatHex(ev.vaddr)} p=${formatHex(ev.paddr)} val=${formatHex(ev.value)} ${ev.cache_used ? (ev.cache_hit ? 'cache-hit' : 'cache-miss') : 'direct'}`;
      dom.memEventList.appendChild(li);
    }
  }
}

function getPerfWindow(frame) {
  if (!frame || state.cursor < 0) {
    return null;
  }

  const endIdx = state.cursor;
  const minCycle = Math.max(0, (frame.cycle || 0) - (state.perfWindowCycles - 1));
  let startIdx = endIdx;
  while (startIdx > 0 && state.traces[startIdx - 1].cycle >= minCycle) {
    startIdx -= 1;
  }

  const frames = state.traces.slice(startIdx, endIdx + 1);
  if (frames.length === 0) {
    return null;
  }

  return { startIdx, frames };
}

function renderPerformance(frame) {
  const svg = dom.perfSvg;
  const node = svg.node();
  const width = Math.max(290, node.clientWidth || 290);
  const height = 220;
  svg.attr('viewBox', `0 0 ${width} ${height}`);
  svg.selectAll('*').remove();

  const windowData = getPerfWindow(frame);
  if (!windowData || windowData.frames.length < 2) {
    svg.append('text')
      .attr('x', 14)
      .attr('y', 28)
      .attr('fill', '#6c726c')
      .attr('font-family', 'IBM Plex Mono, monospace')
      .attr('font-size', 11)
      .text('等待足够的周期数据...');
    return;
  }

  const { startIdx, frames } = windowData;
  const points = frames.map((f, localIdx) => {
    const globalIdx = startIdx + localIdx;
    const prev = globalIdx > 0 ? state.traces[globalIdx - 1] : null;

    const cycle = typeof f.cycle === 'number' ? f.cycle : 0;
    const prevCycle = prev && typeof prev.cycle === 'number' ? prev.cycle : (cycle - 1);
    const deltaCycle = Math.max(1, cycle - prevCycle);

    const retired = typeof f.instrs_retired === 'number' ? f.instrs_retired : 0;
    const prevRetired = prev && typeof prev.instrs_retired === 'number' ? prev.instrs_retired : retired;
    const ipc = clamp((retired - prevRetired) / deltaCycle, 0, 4);

    const metrics = f.metrics || {};
    const dAcc = typeof metrics.dcache_accesses === 'number' ? metrics.dcache_accesses : 0;
    const dHits = typeof metrics.dcache_hits === 'number' ? metrics.dcache_hits : 0;
    const dRate = dAcc > 0 ? clamp(dHits / dAcc, 0, 1) : 0;

    return { cycle, ipc, dRate };
  });

  const margin = { top: 14, right: 40, bottom: 30, left: 38 };
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;

  const minCycle = points[0].cycle;
  const maxCycle = points[points.length - 1].cycle;

  const x = d3.scaleLinear()
    .domain([minCycle, Math.max(minCycle + 1, maxCycle)])
    .range([margin.left, margin.left + plotW]);

  const maxIpc = Math.max(1, d3.max(points, (d) => d.ipc) || 1);
  const yIpc = d3.scaleLinear()
    .domain([0, maxIpc])
    .nice()
    .range([margin.top + plotH, margin.top]);

  const yRate = d3.scaleLinear()
    .domain([0, 1])
    .range([margin.top + plotH, margin.top]);

  svg.append('g')
    .attr('transform', `translate(0, ${margin.top + plotH})`)
    .call(d3.axisBottom(x).ticks(5).tickFormat((v) => `${v}`))
    .call((g) => g.selectAll('text').attr('font-family', 'IBM Plex Mono, monospace').attr('font-size', 10));

  svg.append('g')
    .attr('transform', `translate(${margin.left}, 0)`)
    .call(d3.axisLeft(yIpc).ticks(5))
    .call((g) => g.selectAll('text').attr('font-family', 'IBM Plex Mono, monospace').attr('font-size', 10));

  svg.append('g')
    .attr('transform', `translate(${margin.left + plotW}, 0)`)
    .call(d3.axisRight(yRate).ticks(5).tickFormat(d3.format('.0%')))
    .call((g) => g.selectAll('text').attr('font-family', 'IBM Plex Mono, monospace').attr('font-size', 10));

  const ipcLine = d3.line()
    .x((d) => x(d.cycle))
    .y((d) => yIpc(d.ipc))
    .curve(d3.curveMonotoneX);

  const dcacheLine = d3.line()
    .x((d) => x(d.cycle))
    .y((d) => yRate(d.dRate))
    .curve(d3.curveMonotoneX);

  svg.append('path')
    .datum(points)
    .attr('fill', 'none')
    .attr('stroke', '#2f936f')
    .attr('stroke-width', 1.8)
    .attr('d', ipcLine);

  svg.append('path')
    .datum(points)
    .attr('fill', 'none')
    .attr('stroke', '#cf6b34')
    .attr('stroke-width', 1.8)
    .attr('d', dcacheLine);

  const current = points[points.length - 1];
  svg.append('circle')
    .attr('cx', x(current.cycle))
    .attr('cy', yIpc(current.ipc))
    .attr('r', 3)
    .attr('fill', '#2f936f');

  svg.append('circle')
    .attr('cx', x(current.cycle))
    .attr('cy', yRate(current.dRate))
    .attr('r', 3)
    .attr('fill', '#cf6b34');

  svg.append('text')
    .attr('x', margin.left)
    .attr('y', height - 8)
    .attr('fill', '#60675f')
    .attr('font-family', 'IBM Plex Mono, monospace')
    .attr('font-size', 10)
    .text(`IPC ${current.ipc.toFixed(2)} · D-hit ${formatPct(current.dRate)}`);
}

function renderVirtualConsole() {
  dom.virtualConsole.textContent = state.virtualConsoleText;
  dom.virtualConsole.scrollTop = dom.virtualConsole.scrollHeight;
}

function computeMemoryWindow(frame) {
  const base = state.memWindowBase >>> 0;
  const size = clamp(state.memWindowSize | 0, 16, 512);
  const end = (base + size) >>> 0;

  const known = new Array(size).fill(false);
  const values = new Array(size).fill(0);
  const hot = new Set();

  const cycle = frame ? frame.cycle : -1;
  const recentWrites = [];
  const includeMmio = state.includeMmioStores;

  for (const w of state.memWrites) {
    if (cycle >= 0 && w.cycle > cycle) break;
    if (!includeMmio && w.mmio) continue;

    const wStart = w.paddr >>> 0;
    const wEnd = (wStart + w.size) >>> 0;
    const overlap = !(wEnd <= base || wStart >= end);
    if (!overlap) continue;

    for (let i = 0; i < w.size; i += 1) {
      const addr = (wStart + i) >>> 0;
      if (addr < base || addr >= end) continue;
      const idx = addr - base;
      values[idx] = w.bytes[i];
      known[idx] = true;
      if (frame && w.cycle === frame.cycle) {
        hot.add(idx);
      }
    }

    recentWrites.push(w);
    if (recentWrites.length > 32) recentWrites.shift();
  }

  return {
    base,
    size,
    values,
    known,
    hot,
    recentWrites
  };
}

function findLatestVisibleStore(frame) {
  const cycle = frame ? frame.cycle : Number.POSITIVE_INFINITY;
  for (let i = state.memWrites.length - 1; i >= 0; i -= 1) {
    const w = state.memWrites[i];
    if (w.cycle > cycle) continue;
    if (!state.includeMmioStores && w.mmio) continue;
    return w;
  }
  return null;
}

function renderMemory(frame) {
  if (state.followLatestStore) {
    const latest = findLatestVisibleStore(frame);
    if (latest) {
      const aligned = (latest.paddr >>> 0) & ~0x3f;
      if (aligned !== state.memWindowBase) {
        state.memWindowBase = aligned;
        dom.memBaseInput.value = formatHex(state.memWindowBase);
      }
    }
  }

  const windowData = computeMemoryWindow(frame);
  const { base, size, values, known, hot, recentWrites } = windowData;

  dom.memRangeLabel.textContent = `range: ${formatHex(base)} .. ${formatHex((base + size - 1) >>> 0)} · known=${known.filter(Boolean).length}/${size}`;

  const rows = [];
  const bytesPerRow = 16;
  for (let row = 0; row < size; row += bytesPerRow) {
    const rowAddr = (base + row) >>> 0;
    const byteParts = [];
    const asciiParts = [];
    for (let col = 0; col < bytesPerRow && (row + col) < size; col += 1) {
      const idx = row + col;
      const isKnown = known[idx];
      const isHot = hot.has(idx);
      const byte = values[idx] & 0xff;

      if (isKnown) {
        const hex = byte.toString(16).padStart(2, '0');
        byteParts.push(`<span class="${isHot ? 'mem-byte-hot' : ''}">${hex}</span>`);
        asciiParts.push(`<span class="${isHot ? 'mem-byte-hot' : ''}">${byteToAscii(byte)}</span>`);
      } else {
        byteParts.push('<span class="mem-byte-unknown">..</span>');
        asciiParts.push('<span class="mem-byte-unknown">.</span>');
      }
    }

    rows.push(`
      <div class="mem-row">
        <span class="mem-addr">${formatHex(rowAddr)}</span>
        <span class="mem-bytes">${byteParts.join(' ')}</span>
        <span class="mem-ascii">${asciiParts.join('')}</span>
      </div>
    `);
  }

  dom.memDump.innerHTML = rows.join('');

  const windowWrites = recentWrites
    .slice(-10)
    .reverse();

  const frameCycle = frame ? frame.cycle : -1;
  const totalVisibleWrites = state.memWrites.reduce((acc, w) => {
    if (frameCycle >= 0 && w.cycle > frameCycle) return acc;
    if (!state.includeMmioStores && w.mmio) return acc;
    return acc + 1;
  }, 0);

  dom.memWriteList.innerHTML = '';
  if (windowWrites.length === 0) {
    if (totalVisibleWrites === 0) {
      dom.memWriteList.innerHTML = '<li class="placeholder">当前周期前没有可回放写入。hello 默认主要是 MMIO 输出，可勾选“包含 MMIO”。</li>';
    } else {
      dom.memWriteList.innerHTML = '<li class="placeholder">当前窗口暂无写入，试试 Base = Last Store。</li>';
    }
  } else {
    for (const w of windowWrites) {
      const li = document.createElement('li');
      li.textContent = `[c${w.cycle}] ${w.type} addr=${formatHex(w.paddr)} size=${w.size}${w.mmio ? ' mmio' : ''}`;
      dom.memWriteList.appendChild(li);
    }
  }
}

function renderLog() {
  dom.log.textContent = state.rawLines.join('\n');
  dom.log.scrollTop = dom.log.scrollHeight;
}

function renderPlaybackControls() {
  dom.playPauseBtn.textContent = state.playing ? 'Pause' : 'Play';
  dom.speedLabel.textContent = `${state.speed.toFixed(2)}x`;
  dom.followTailCheck.checked = state.followTail;
}

function renderAll() {
  ensureCursor();
  const frame = currentFrame();
  renderStatus(frame);
  renderPerformance(frame);
  renderPipeline(frame);
  renderTimeline(frame);
  renderRegs(frame);
  renderCache(frame);
  renderMemory(frame);
  renderVirtualConsole();
  renderLog();
  renderPlaybackControls();
}

function jumpToCycle() {
  if (state.traces.length === 0) return;
  const target = Number(dom.jumpCycleInput.value);
  if (!Number.isFinite(target)) return;

  let idx = state.traces.findIndex((f) => f.cycle >= target);
  if (idx < 0) idx = state.traces.length - 1;
  state.cursor = idx;
  state.followTail = false;
  state.playing = false;
  renderAll();
}

function bindUI() {
  dom.connectBtn.addEventListener('click', connectSSE);
  dom.disconnectBtn.addEventListener('click', disconnectSSE);
  dom.clearBtn.addEventListener('click', clearData);
  dom.clearConsoleBtn.addEventListener('click', () => {
    state.virtualConsoleText = '';
    renderVirtualConsole();
  });

  dom.playPauseBtn.addEventListener('click', () => {
    state.playing = !state.playing;
    if (state.playing) state.lastTick = performance.now();
    renderPlaybackControls();
  });

  dom.stepBackBtn.addEventListener('click', () => {
    if (state.traces.length === 0) return;
    state.playing = false;
    state.followTail = false;
    state.cursor = Math.max(0, state.cursor - 1);
    renderAll();
  });

  dom.stepForwardBtn.addEventListener('click', () => {
    if (state.traces.length === 0) return;
    state.playing = false;
    state.followTail = false;
    state.cursor = Math.min(state.traces.length - 1, state.cursor + 1);
    renderAll();
  });

  dom.speedRange.addEventListener('input', () => {
    state.speed = Number(dom.speedRange.value);
    renderPlaybackControls();
  });

  dom.jumpCycleBtn.addEventListener('click', jumpToCycle);
  dom.jumpCycleInput.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') jumpToCycle();
  });

  dom.followTailCheck.addEventListener('change', () => {
    state.followTail = dom.followTailCheck.checked;
    if (state.followTail && state.traces.length > 0) {
      state.cursor = state.traces.length - 1;
    }
    renderAll();
  });

  const applyMemoryWindow = () => {
    const parsedBase = parseAddr(dom.memBaseInput.value, state.memWindowBase);
    const parsedSize = clamp(Number(dom.memSizeInput.value) || state.memWindowSize, 16, 512);
    state.memWindowBase = parsedBase;
    state.memWindowSize = parsedSize;
    dom.memBaseInput.value = formatHex(state.memWindowBase);
    dom.memSizeInput.value = String(state.memWindowSize);
    renderMemory(currentFrame());
  };

  dom.memApplyBtn.addEventListener('click', applyMemoryWindow);
  dom.memBaseInput.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') applyMemoryWindow();
  });
  dom.memSizeInput.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') applyMemoryWindow();
  });
  dom.memAtPcBtn.addEventListener('click', () => {
    const frame = currentFrame();
    if (!frame || typeof frame.pc !== 'number') return;
    state.memWindowBase = (frame.pc >>> 0) & ~0x3f;
    dom.memBaseInput.value = formatHex(state.memWindowBase);
    renderMemory(frame);
  });

  dom.memLastWriteBtn.addEventListener('click', () => {
    const frame = currentFrame();
    const latest = findLatestVisibleStore(frame);
    if (!latest) return;
    state.memWindowBase = (latest.paddr >>> 0) & ~0x3f;
    dom.memBaseInput.value = formatHex(state.memWindowBase);
    renderMemory(frame);
  });

  dom.memIncludeMmioCheck.addEventListener('change', () => {
    state.includeMmioStores = !!dom.memIncludeMmioCheck.checked;
    renderMemory(currentFrame());
  });

  dom.memFollowLastWriteCheck.addEventListener('change', () => {
    state.followLatestStore = !!dom.memFollowLastWriteCheck.checked;
    renderMemory(currentFrame());
  });

  window.addEventListener('resize', () => {
    renderPerformance(currentFrame());
    renderPipeline(currentFrame());
    renderTimeline(currentFrame());
  });
}

function animationLoop(now) {
  if (state.playing && state.traces.length > 0) {
    const framesPerSecond = Math.max(0.5, state.speed * 8);
    const frameDuration = 1000 / framesPerSecond;
    if (now - state.lastTick >= frameDuration) {
      const steps = Math.floor((now - state.lastTick) / frameDuration);
      state.lastTick += steps * frameDuration;

      if (state.followTail) {
        state.cursor = state.traces.length - 1;
      } else if (state.cursor < state.traces.length - 1) {
        state.cursor = Math.min(state.traces.length - 1, state.cursor + steps);
      }
      renderAll();
    }
  }
  requestAnimationFrame(animationLoop);
}

function boot() {
  bindUI();
  dom.memBaseInput.value = formatHex(state.memWindowBase);
  dom.memSizeInput.value = String(state.memWindowSize);
  dom.memIncludeMmioCheck.checked = state.includeMmioStores;
  dom.memFollowLastWriteCheck.checked = state.followLatestStore;
  setConnected(false);
  renderAll();
  requestAnimationFrame(animationLoop);
}

boot();
