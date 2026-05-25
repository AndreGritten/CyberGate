// API Base URLs
const API_STATUS = '/api/status';
const API_CONTROL = '/api/control';
const API_LOGS = '/api/logs';
const API_PERFORMANCE = '/api/performance';

let memoryChart = null;

function formatTimeMs(usec) {
    return (usec / 1000).toFixed(2) + ' ms';
}

// Função para buscar status do sistema para Dashboard e Métricas
async function fetchStatus() {
    try {
        const res = await fetch(API_STATUS);
        if(!res.ok) throw new Error("Erro de rede");
        const data = await res.json();
        
        // Atualiza elementos do Dashboard (se existirem na página)
        const gateEl = document.getElementById('gateStatus');
        if(gateEl) {
            gateEl.textContent = data.isGateOpen ? "ABERTO" : "FECHADO";
            gateEl.className = 'status-indicator ' + (data.isGateOpen ? 'success' : 'danger');
        }

        const vehEl = document.getElementById('vehicleStatus');
        if(vehEl) {
            vehEl.textContent = data.vehicleDetected ? "CARRO AGUARDANDO" : "LIVRE";
            vehEl.className = 'status-indicator ' + (data.vehicleDetected ? 'danger' : 'success');
        }

        // Atualiza elementos de Métricas (se existirem na página)
        const upEl = document.getElementById('uptimeVal');
        if(upEl) upEl.textContent = data.uptime;
        
        const heapEl = document.getElementById('heapVal');
        if(heapEl) heapEl.textContent = data.memoryFree;
        
        const reqEl = document.getElementById('reqTimeVal');
        if(reqEl) reqEl.textContent = data.requestTime;

    } catch(err) {
        console.error("Falha ao buscar status", err);
    }
}

async function fetchPerformance() {
    try {
        const res = await fetch(API_PERFORMANCE);
        if (!res.ok) throw new Error('Erro rede');
        const data = await res.json();

        const upEl = document.getElementById('uptimeVal');
        if (upEl) upEl.textContent = data.uptime;
        const heapEl = document.getElementById('heapVal');
        if (heapEl) heapEl.textContent = data.memoryFree;
        const reqEl = document.getElementById('reqTimeVal');
        if (reqEl) reqEl.textContent = data.lastRequestTimeMs;

        // tabela de funções
        const functionBody = document.getElementById('perfTableBody');
        if (functionBody) {
            functionBody.innerHTML = '';
            data.functions.forEach(fn => {
                const tr = document.createElement('tr');
                tr.innerHTML = `<td>${fn.name}</td><td>${fn.calls}</td><td>${formatTimeMs(fn.lastUs)}</td><td>${formatTimeMs(fn.avgUs)}</td><td>${formatTimeMs(fn.maxUs)}</td>`;
                functionBody.appendChild(tr);
            });
        }

        // gráfico de memória (Chart.js)
        const ctx = document.getElementById('memoryChartCanvas');
        if (ctx) {
            const labels = data.memoryHistory.map((_,i) => (i+1).toString());
            const dataset = data.memoryHistory;
            if (!memoryChart) {
                memoryChart = new Chart(ctx.getContext('2d'), {
                    type: 'line',
                    data: {
                        labels: labels,
                        datasets: [{ label: 'Heap livre (bytes)', data: dataset, borderColor: '#10b981', backgroundColor: 'rgba(16,185,129,0.15)', tension: 0.3 }]
                    },
                    options: { scales: { y: { beginAtZero: true } } }
                });
            } else {
                memoryChart.data.labels = labels;
                memoryChart.data.datasets[0].data = dataset;
                memoryChart.update();
            }
        }

    } catch(err) {
        console.error('Falha ao buscar performance', err);
    }
}

// Função para disparar a ação do portão
async function openGate() {
    const btn = document.getElementById('btnOpenGate');
    const fb = document.getElementById('actionFeedback');
    
    btn.disabled = true;
    btn.textContent = "ENVIANDO...";

    try {
        const res = await fetch(API_CONTROL + '?action=openGate', { method: 'POST' });
        const data = await res.json();
        
        if (data.status === 'success') {
            fb.innerHTML = "<p style='color:var(--accent); margin-top:1rem;'>Comando processado com sucesso pelo FreeRTOS!</p>";
        } else {
            fb.innerHTML = "<p style='color:var(--danger); margin-top:1rem;'>Erro no comando.</p>";
        }
    } catch(err) {
        fb.innerHTML = "<p style='color:var(--danger); margin-top:1rem;'>Erro ao comunicar com o servidor.</p>";
    }
    
    setTimeout(() => {
        btn.disabled = false;
        btn.textContent = "ABRIR PORTÃO (SIMULAR)";
        fb.innerHTML = "";
    }, 3000);
}

// Função para buscar e renderizar Logs
async function fetchLogs() {
    try {
        const res = await fetch(API_LOGS);
        if(!res.ok) throw new Error();
        const data = await res.json();
        
        const ul = document.getElementById('logList');
        if(ul) {
            ul.innerHTML = ''; // Limpa antigos
            if (data.logs.length === 0) {
                ul.innerHTML = '<li>Sem eventos recentes...</li>';
                return;
            }
            // Invertemos para o mais recente aparecer no topo (se o append for padrao)
            data.logs.reverse().forEach(log => {
                const li = document.createElement('li');
                
                // Formatação simples de tempo (em millis, podemos transformar em "X seg atrás" se preferir)
                const timeSt = (log.timestamp / 1000).toFixed(1) + "s";
                
                li.innerHTML = `<span class="badge">${log.type} [${timeSt}]</span> ${log.message}`;
                ul.appendChild(li);
            });
        }

    } catch(err) {
        console.error("Falha ao puxar logs");
    }
}
