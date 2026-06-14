const API_STATUS = '/api/status';
const API_CONTROL = '/api/control';
const API_LOGS = '/api/logs';
const API_PERFORMANCE = '/api/performance';

const chartColors = {
    grid: 'rgba(156, 163, 175, 0.18)',
    text: '#9ca3af',
    memory: '#22c55e',
    function: '#ef4444',
    cpu: '#f59e0b',
    psram: '#38bdf8',
    sensor: '#ef4444',
    rfid: '#a855f7'
};

const performanceHistory = {
    cpu: [],
    heap: [],
    psram: [],
    distance: [],
    rfid: []
};
const MAX_LOCAL_HISTORY = 40;

function setText(id, value) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
}

function setStatusClass(el, state) {
    if (!el) return;
    el.className = 'status-chip ' + state;
}

function formatTimeMs(usec) {
    return ((Number(usec) || 0) / 1000).toFixed(2) + ' ms';
}

function formatBytes(bytes) {
    if (bytes === undefined || bytes === null) return '--';
    const value = Number(bytes) || 0;
    if (value >= 1024 * 1024) return (value / (1024 * 1024)).toFixed(2) + ' MB';
    if (value >= 1024) return (value / 1024).toFixed(1) + ' KB';
    return value + ' B';
}

function formatPercent(value) {
    if (value === undefined || value === null) return '--';
    return Number(value).toFixed(1) + '%';
}

function formatSeconds(ms) {
    if (!ms) return 'sem leitura';
    return (ms / 1000).toFixed(1) + 's';
}

function pushHistory(key, value) {
    performanceHistory[key].push(Number(value) || 0);
    if (performanceHistory[key].length > MAX_LOCAL_HISTORY) {
        performanceHistory[key].shift();
    }
}

function setupCanvas(canvas) {
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.floor(rect.width * dpr));
    canvas.height = Math.max(1, Math.floor(rect.height * dpr));

    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return { ctx, width: rect.width, height: rect.height };
}

function drawEmptyChart(canvas, message) {
    const { ctx, width, height } = setupCanvas(canvas);
    ctx.clearRect(0, 0, width, height);
    ctx.fillStyle = chartColors.text;
    ctx.font = '14px system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(message, width / 2, height / 2);
}

function drawMemoryChart(samples) {
    const canvas = document.getElementById('memoryChartCanvas');
    if (!canvas) return;
    if (!samples || samples.length === 0) {
        drawEmptyChart(canvas, 'Aguardando amostras de memoria');
        return;
    }

    const { ctx, width, height } = setupCanvas(canvas);
    const pad = { top: 18, right: 18, bottom: 28, left: 58 };
    const chartWidth = width - pad.left - pad.right;
    const chartHeight = height - pad.top - pad.bottom;
    const min = Math.min(...samples);
    const max = Math.max(...samples);
    const range = Math.max(1, max - min);

    ctx.clearRect(0, 0, width, height);
    ctx.strokeStyle = chartColors.grid;
    ctx.lineWidth = 1;
    ctx.fillStyle = chartColors.text;
    ctx.font = '12px system-ui, sans-serif';
    ctx.textAlign = 'right';

    for (let i = 0; i <= 4; i++) {
        const y = pad.top + (chartHeight / 4) * i;
        const value = max - (range / 4) * i;
        ctx.beginPath();
        ctx.moveTo(pad.left, y);
        ctx.lineTo(width - pad.right, y);
        ctx.stroke();
        ctx.fillText(formatBytes(Math.round(value)), pad.left - 8, y + 4);
    }

    ctx.strokeStyle = chartColors.memory;
    ctx.lineWidth = 3;
    ctx.beginPath();
    samples.forEach((value, index) => {
        const x = pad.left + (samples.length === 1 ? chartWidth : (chartWidth / (samples.length - 1)) * index);
        const y = pad.top + chartHeight - ((value - min) / range) * chartHeight;
        if (index === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    });
    ctx.stroke();
}

function drawMultiLineChart(canvasId, series, options = {}) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    const activeSeries = series.filter(item => item.values && item.values.length > 0);
    if (activeSeries.length === 0) {
        drawEmptyChart(canvas, 'Aguardando amostras');
        return;
    }

    const { ctx, width, height } = setupCanvas(canvas);
    const pad = { top: 18, right: 20, bottom: 34, left: 62 };
    const chartWidth = width - pad.left - pad.right;
    const chartHeight = height - pad.top - pad.bottom;
    const allValues = activeSeries.flatMap(item => item.values);
    const min = options.min ?? Math.min(...allValues);
    const max = options.max ?? Math.max(...allValues);
    const range = Math.max(1, max - min);

    ctx.clearRect(0, 0, width, height);
    ctx.strokeStyle = chartColors.grid;
    ctx.lineWidth = 1;
    ctx.fillStyle = chartColors.text;
    ctx.font = '12px system-ui, sans-serif';
    ctx.textAlign = 'right';

    for (let i = 0; i <= 4; i++) {
        const y = pad.top + (chartHeight / 4) * i;
        const value = max - (range / 4) * i;
        ctx.beginPath();
        ctx.moveTo(pad.left, y);
        ctx.lineTo(width - pad.right, y);
        ctx.stroke();
        ctx.fillText(options.format ? options.format(value) : value.toFixed(0), pad.left - 8, y + 4);
    }

    activeSeries.forEach(item => {
        ctx.strokeStyle = item.color;
        ctx.lineWidth = 2.5;
        ctx.beginPath();
        item.values.forEach((value, index) => {
            const x = pad.left + (item.values.length === 1 ? chartWidth : (chartWidth / (item.values.length - 1)) * index);
            const y = pad.top + chartHeight - ((value - min) / range) * chartHeight;
            if (index === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });
        ctx.stroke();
    });

    ctx.textAlign = 'left';
    activeSeries.forEach((item, index) => {
        const x = pad.left + index * 112;
        const y = height - 12;
        ctx.fillStyle = item.color;
        ctx.fillRect(x, y - 8, 10, 10);
        ctx.fillStyle = chartColors.text;
        ctx.fillText(item.label, x + 16, y - 2);
    });
}

function drawFunctionChart(functions) {
    const canvas = document.getElementById('functionChartCanvas');
    if (!canvas) return;
    if (!functions || functions.length === 0) {
        drawEmptyChart(canvas, 'Aguardando chamadas monitoradas');
        return;
    }

    const { ctx, width, height } = setupCanvas(canvas);
    const values = functions.map(fn => (fn.avgUs || 0) / 1000);
    const max = Math.max(0.01, ...values);
    const pad = { top: 18, right: 18, bottom: 42, left: 110 };
    const chartWidth = width - pad.left - pad.right;
    const rowHeight = (height - pad.top - pad.bottom) / functions.length;

    ctx.clearRect(0, 0, width, height);
    ctx.font = '12px system-ui, sans-serif';
    ctx.textBaseline = 'middle';

    functions.forEach((fn, index) => {
        const valueMs = values[index];
        const y = pad.top + rowHeight * index + rowHeight / 2;
        const barWidth = Math.max(2, (valueMs / max) * chartWidth);

        ctx.fillStyle = chartColors.text;
        ctx.textAlign = 'right';
        ctx.fillText(fn.name, pad.left - 10, y);

        ctx.fillStyle = 'rgba(239, 68, 68, 0.16)';
        ctx.fillRect(pad.left, y - 10, chartWidth, 20);

        ctx.fillStyle = chartColors.function;
        ctx.fillRect(pad.left, y - 10, barWidth, 20);

        ctx.fillStyle = '#f8fafc';
        ctx.textAlign = 'left';
        const labelX = Math.min(pad.left + barWidth + 8, width - pad.right - 52);
        ctx.fillText(valueMs.toFixed(2) + ' ms', labelX, y);
    });
}

function updateMetricList(id, items) {
    const el = document.getElementById(id);
    if (!el) return;
    el.innerHTML = '';
    items.forEach(item => {
        const row = document.createElement('div');
        row.className = 'metric-list-row';
        const label = document.createElement('span');
        const value = document.createElement('strong');
        label.textContent = item.label;
        value.textContent = item.value;
        row.appendChild(label);
        row.appendChild(value);
        el.appendChild(row);
    });
}

function renderTaskTable(tasks) {
    const body = document.getElementById('taskTableBody');
    if (!body) return;
    body.innerHTML = '';
    (tasks?.items || []).forEach(task => {
        const tr = document.createElement('tr');
        [
            task.name,
            task.priority,
            task.core,
            task.periodMs ? task.periodMs + ' ms' : 'sob demanda',
            task.stackFree ? formatBytes(task.stackFree) : '--'
        ].forEach(value => {
            const td = document.createElement('td');
            td.textContent = value;
            tr.appendChild(td);
        });
        body.appendChild(tr);
    });
}

function updateGateScene(data) {
    const scenes = [
        document.getElementById('gateScene'),
        document.getElementById('controlGateScene')
    ].filter(Boolean);

    scenes.forEach(scene => {
        scene.classList.toggle('gate-open', !!data.isGateOpen);
        scene.classList.toggle('vehicle-waiting', !!data.vehicleDetected);
    });

    const gateEl = document.getElementById('gateStatus');
    if (gateEl) {
        gateEl.textContent = data.isGateOpen ? 'Portao aberto' : 'Portao fechado';
        setStatusClass(gateEl, data.isGateOpen ? 'success' : 'danger');
    }

    setText('gateHeadline', data.isGateOpen ? 'Portao aberto para passagem' : 'Portao fechado e protegido');
    setText('gateSubline', data.vehicleDetected
        ? 'Veiculo detectado pelo ultrassonico. RFID pronto para validar a tag.'
        : 'Nenhum veiculo aguardando no sensor ultrassonico.');

    const pulse = document.getElementById('systemPulse');
    if (pulse) {
        pulse.textContent = data.vehicleDetected ? 'Veiculo em espera' : 'Sistema online';
        pulse.className = 'system-pill ' + (data.vehicleDetected ? 'warning' : 'success');
    }
}

function updateRfidAttempt(data) {
    const status = data.lastRfidStatus || 'none';
    const statusEl = document.getElementById('rfidAttemptStatus');
    const detailEl = document.getElementById('rfidAttemptDetail');
    if (!statusEl && !detailEl) return;

    const uid = data.lastUid || '--';
    const at = formatSeconds(data.lastRfidAt || 0);

    if (status === 'authorized') {
        setText('rfidAttemptStatus', 'Autorizada');
        if (statusEl) statusEl.className = 'metric-value compact-value success-text';
        setText('rfidAttemptDetail', `${data.lastRfidMessage || 'Tag autorizada'} | UID ${uid} | ${at}`);
    } else if (status === 'denied') {
        setText('rfidAttemptStatus', 'Negada');
        if (statusEl) statusEl.className = 'metric-value compact-value danger-text';
        setText('rfidAttemptDetail', `${data.lastRfidMessage || 'Tag negada'} | UID ${uid} | ${at}`);
    } else {
        setText('rfidAttemptStatus', 'Aguardando');
        if (statusEl) statusEl.className = 'metric-value compact-value muted-text';
        setText('rfidAttemptDetail', 'Nenhuma tag validada ainda');
    }
}

async function fetchStatus() {
    try {
        const res = await fetch(API_STATUS);
        if (!res.ok) throw new Error('Erro de rede');
        const data = await res.json();

        updateGateScene(data);
        updateRfidAttempt(data);

        setText('vehicleStatus', data.vehicleDetected ? 'Aguardando' : 'Livre');
        setText('vehicleDetail', `Distancia: ${Number(data.distance || 0).toFixed(1)} cm`);
        setText('rfidStatus', data.rfidActive ? 'Ativo' : 'Inativo');
        setText('rfidDetail', `UID: ${data.lastUid || '--'}`);
        setText('uptimeVal', `${data.uptime || 0}s`);
        setText('heapVal', `Heap: ${formatBytes(data.memoryFree)}`);
    } catch(err) {
        console.error('Falha ao buscar status', err);
        const gateEl = document.getElementById('gateStatus');
        if (gateEl) {
            gateEl.textContent = 'Sem conexao';
            setStatusClass(gateEl, 'warning');
        }
    }
}

async function fetchPerformance() {
    try {
        const res = await fetch(API_PERFORMANCE);
        if (!res.ok) throw new Error('Erro rede');
        const data = await res.json();

        setText('uptimeVal', data.uptime);
        setText('heapVal', formatBytes(data.memory?.heapFree ?? data.memoryFree));
        setText('cpuVal', formatPercent(data.cpuLoadPct));
        setText('cpuFreqVal', (data.cpuFreqMhz || '--') + ' MHz');
        setText('reqTimeVal', data.lastRequestTimeMs);
        setText('wifiStatusVal', data.wifi?.status || '--');
        setText('wifiDetailVal', `${data.wifi?.localIp || '--'} | RSSI ${data.wifi?.rssi ?? '--'} dBm`);
        setText('sensorVal', data.sensors?.vehicleDetected ? 'Veiculo' : 'Livre');
        setText('sensorDetailVal', `${Number(data.sensors?.distanceCm || 0).toFixed(1)} cm | RFID ${data.sensors?.rfidActive ? 'ativo' : 'inativo'}`);

        updateMetricList('resourceList', [
            { label: 'Heap total', value: formatBytes(data.memory?.heapTotal) },
            { label: 'Heap livre', value: formatBytes(data.memory?.heapFree) },
            { label: 'Menor heap livre', value: formatBytes(data.memory?.heapMinFree) },
            { label: 'Maior bloco heap', value: formatBytes(data.memory?.heapMaxAlloc) },
            { label: 'PSRAM total', value: formatBytes(data.memory?.psramTotal) },
            { label: 'PSRAM livre', value: formatBytes(data.memory?.psramFree) },
            { label: 'Flash chip', value: formatBytes(data.memory?.flashSize) },
            { label: 'Sketch', value: formatBytes(data.memory?.sketchSize) },
            { label: 'Espaco OTA livre', value: formatBytes(data.memory?.sketchFree) },
            { label: 'LittleFS usado', value: `${formatBytes(data.memory?.littleFsUsed)} / ${formatBytes(data.memory?.littleFsTotal)}` },
            { label: 'Stack livre SensorTask', value: formatBytes(data.memory?.sensorStackFree) },
            { label: 'Stack livre ControlTask', value: formatBytes(data.memory?.controlStackFree) }
        ]);

        updateMetricList('wifiList', [
            { label: 'Status', value: data.wifi?.status || '--' },
            { label: 'SSID', value: data.wifi?.ssid || '--' },
            { label: 'IP local', value: data.wifi?.localIp || '--' },
            { label: 'RSSI', value: `${data.wifi?.rssi ?? '--'} dBm` },
            { label: 'MAC', value: data.wifi?.mac || '--' },
            { label: 'Modo Wi-Fi', value: data.wifi?.mode ?? '--' },
            { label: 'IP do AP', value: data.wifi?.apIp || '--' },
            { label: 'Clientes no AP', value: data.wifi?.apStations ?? '--' }
        ]);

        renderTaskTable(data.tasks);

        pushHistory('cpu', data.cpuLoadPct);
        pushHistory('heap', data.memory?.heapFree || data.memoryFree);
        pushHistory('psram', data.memory?.psramFree || 0);
        pushHistory('distance', data.sensors?.distanceCm || 0);
        pushHistory('rfid', data.sensors?.rfidActive ? 1 : 0);

        const functionBody = document.getElementById('perfTableBody');
        if (functionBody) {
            functionBody.innerHTML = '';
            (data.functions || []).forEach(fn => {
                const tr = document.createElement('tr');
                [fn.name, fn.calls, formatTimeMs(fn.lastUs), formatTimeMs(fn.avgUs), formatTimeMs(fn.maxUs)].forEach(value => {
                    const td = document.createElement('td');
                    td.textContent = value;
                    tr.appendChild(td);
                });
                functionBody.appendChild(tr);
            });
        }

        drawMemoryChart(data.memoryHistory || []);
        drawFunctionChart(data.functions || []);
        drawMultiLineChart('resourceChartCanvas', [
            { label: 'CPU %', color: chartColors.cpu, values: performanceHistory.cpu },
            { label: 'Heap KB', color: chartColors.memory, values: performanceHistory.heap.map(value => value / 1024) },
            { label: 'PSRAM KB', color: chartColors.psram, values: performanceHistory.psram.map(value => value / 1024) }
        ], { format: value => value.toFixed(0) });
        drawMultiLineChart('sensorChartCanvas', [
            { label: 'Distancia cm', color: chartColors.sensor, values: performanceHistory.distance },
            { label: 'RFID ativo', color: chartColors.rfid, values: performanceHistory.rfid.map(value => value * 10) }
        ], { min: 0, format: value => value.toFixed(0) });
    } catch(err) {
        console.error('Falha ao buscar performance', err);
    }
}

async function openGate() {
    const btn = document.getElementById('btnOpenGate');
    const fb = document.getElementById('actionFeedback');
    if (!btn) return;

    btn.disabled = true;
    btn.textContent = 'Enviando...';
    if (fb) fb.textContent = '';

    try {
        const res = await fetch(API_CONTROL + '?action=openGate', { method: 'POST' });
        const data = await res.json();

        if (fb) {
            const p = document.createElement('p');
            p.className = data.status === 'success' ? 'success-text' : 'danger-text';
            p.textContent = data.status === 'success'
                ? 'Comando enviado para a task de controle.'
                : 'Erro ao processar comando.';
            fb.appendChild(p);
        }
        await fetchStatus();
    } catch(err) {
        if (fb) {
            const p = document.createElement('p');
            p.className = 'danger-text';
            p.textContent = 'Erro ao comunicar com o servidor.';
            fb.appendChild(p);
        }
    }

    setTimeout(() => {
        btn.disabled = false;
        btn.textContent = 'Abrir portao';
        if (fb) fb.textContent = '';
    }, 3000);
}

function badgeClass(type) {
    const normalized = String(type || '').toLowerCase();
    if (normalized.includes('rfid')) return 'badge badge-rfid';
    if (normalized.includes('sensor')) return 'badge badge-sensor';
    if (normalized.includes('controle')) return 'badge badge-controle';
    if (normalized.includes('usuario')) return 'badge badge-usuario';
    if (normalized.includes('erro')) return 'badge badge-erro';
    return 'badge';
}

async function fetchLogs() {
    try {
        const res = await fetch(API_LOGS);
        if (!res.ok) throw new Error();
        const data = await res.json();

        const ul = document.getElementById('logList');
        if (!ul) return;
        ul.innerHTML = '';
        if (!data.logs || data.logs.length === 0) {
            const li = document.createElement('li');
            li.textContent = 'Sem eventos recentes...';
            ul.appendChild(li);
            return;
        }

        data.logs.slice().reverse().forEach(log => {
            const li = document.createElement('li');
            const badge = document.createElement('span');
            const message = document.createElement('span');
            const timeSt = (log.timestamp / 1000).toFixed(1) + 's';

            badge.className = badgeClass(log.type);
            badge.textContent = `${log.type} ${timeSt}`;
            message.textContent = log.message;
            li.appendChild(badge);
            li.appendChild(message);
            ul.appendChild(li);
        });
    } catch(err) {
        console.error('Falha ao puxar logs');
    }
}
