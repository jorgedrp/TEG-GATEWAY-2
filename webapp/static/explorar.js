document.addEventListener('DOMContentLoaded', () => {

    // 1. Leer los parámetros de la URL
    const parametrosURL = new URLSearchParams(window.location.search);
    const inicioMs = parametrosURL.get('inicio');
    const finMs = parametrosURL.get('fin');
    const sensorid = parametrosURL.get('sensor');

    // 2. Función para convertir timestamp Unix (ms) a formato datetime-local
    function formatearParaInput(timestampMs) {
        if (!timestampMs) return "";
        
        const fecha = new Date(parseInt(timestampMs));
        
        // Ajuste de zona horaria para que el input muestre la hora local correcta del usuario
        // y no se desplace por la conversión a UTC de toISOString()
        const offsetMinutos = fecha.getTimezoneOffset() * 60000;
        const fechaLocal = new Date(fecha.getTime() - offsetMinutos);
        
        // Recortamos el string para que encaje perfectamente en el formato "YYYY-MM-DDThh:mm:ss"
        return fechaLocal.toISOString().slice(0, 19);
    }

    // 3. Asignar los valores a los inputs si existen en la URL
    if (inicioMs) {
        document.getElementById('input-inicio').value = formatearParaInput(inicioMs);
    }
    
    if (finMs) {
        document.getElementById('input-fin').value = formatearParaInput(finMs);
    }

    if (sensorid) {
        document.getElementById('select-sensor').value = sensorid;
    }
    
    const existingScript = document.querySelector('script[src*="chart.js"]');
    const chartJsUrl = existingScript ? existingScript.src : '/static/chart.js'; // fallback

    let timeChart = null;
    let frequencyChart = null;
    let pitchChart = null;
    let rollChart = null;
    let temperatureChart = null;
    let humidityChart = null;
    let pressureChart = null;

    // --- Lógica del formulario (Botones de Zoom/Expansión) ---
    const form = document.getElementById('graficar-form');
    const minFreqInput = document.getElementById('minFreq');
    const maxFreqInput = document.getElementById('maxFreq');
    const updateBtn = document.getElementById('updateRangeBtn');
    const resetBtn = document.getElementById('resetZoomBtn');
    const expandButton = document.getElementById('expand-freq-chart-btn');
    const expandTimeButton = document.getElementById('expand-time-chart-btn');

    updateBtn.addEventListener('click', () => {
        const newMin = parseFloat(minFreqInput.value);
        const newMax = parseFloat(maxFreqInput.value);
        if (!isNaN(newMin) && !isNaN(newMax) && newMin < newMax) {
            frequencyChart.options.scales.x.min = newMin;
            frequencyChart.options.scales.x.max = newMax;
            frequencyChart.update();
        } else {
            alert("Por favor, introduce un rango válido.");
        }
    });

    resetBtn.addEventListener('click', () => {
        frequencyChart.options.scales.x.min = undefined;
        frequencyChart.options.scales.x.max = undefined;
        frequencyChart.update();
    });

    expandTimeButton.addEventListener('click', () => {
        if (!timeChart) {
            alert("Primero debes generar un gráfico para poder expandirlo.");
            return;
        }
        const newWindow = window.open('', 'TimeChartWindow', 'width=1000,height=700');
        if (!newWindow) {
            alert("El navegador ha bloqueado la ventana emergente. Permite pop-ups para este sitio.");
            return;
        }
        
        // Copia profunda de datos y opciones específicas del gráfico de tiempo
        const dataCopy = JSON.parse(JSON.stringify(timeChart.data));
        const srcOpts = timeChart.options || {};
        const xScale = (srcOpts.scales && srcOpts.scales.x) || {};
        const yScale = (srcOpts.scales && srcOpts.scales.y) || {};
    
        const optionsCopy = {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    type: xScale.type || 'category',
                    title: (xScale.title && { display: !!(xScale.title && xScale.title.display), text: (xScale.title && xScale.title.text) || '' }) || undefined,
                    ticks: (xScale.ticks && { maxTicksLimit: xScale.ticks.maxTicksLimit || 10 }) || undefined
                },
                y: {
                    title: (yScale.title && { display: !!(yScale.title && yScale.title.display), text: (yScale.title && yScale.title.text) || '' }) || undefined,
                    beginAtZero: !!yScale.beginAtZero
                }
            }
        };
    
        const html = `
            <!doctype html>
            <html>
            <head>
            <meta charset="utf-8" />
            <title>Gráfico de Tiempo (Expandido)</title>
            <style>
              html,body { height:100%; margin:0; background:#f6f7fb; font-family: Inter, sans-serif; }
              .chart-popup-container { width: 100vw; height: 100vh; display:flex; align-items:center; justify-content:center; padding:16px; box-sizing:border-box; }
              canvas { width:100%; height:100%; }
            </style>
            </head>
            <body>
              <div class="chart-popup-container"><canvas id="popupTimeChart"></canvas></div>
              <script src="${chartJsUrl}"></script>
              <script>
                (function() {
                  const chartData = ${JSON.stringify(dataCopy)};
                  const chartOptions = ${JSON.stringify(optionsCopy)};
                  function tryCreate() {
                    if (window.Chart) {
                      const ctx = document.getElementById('popupTimeChart').getContext('2d');
                      new Chart(ctx, { type: 'line', data: chartData, options: chartOptions });
                    } else { setTimeout(tryCreate, 50); }
                  }
                  tryCreate();
                })();
              </script>
            </body>
            </html>`;
        newWindow.document.open();
        newWindow.document.write(html);
        newWindow.document.close();
    });

    expandButton.addEventListener('click', () => {
        if (!frequencyChart) {
            alert("Primero debes generar un gráfico para poder expandirlo.");
            return;
        }
        const newWindow = window.open('', 'ChartWindow', 'width=1000,height=700');
        if (!newWindow) {
            alert("El navegador ha bloqueado la ventana emergente. Permite pop-ups para este sitio.");
            return;
        }
        
        // Copia profunda de datos y opciones para la ventana emergente
        const dataCopy = JSON.parse(JSON.stringify(frequencyChart.data));
        const srcOpts = frequencyChart.options || {};
        const xScale = (srcOpts.scales && srcOpts.scales.x) || {};
        const yScale = (srcOpts.scales && srcOpts.scales.y) || {};

        const optionsCopy = {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    type: xScale.type || 'linear',
                    title: (xScale.title && { display: !!(xScale.title && xScale.title.display), text: (xScale.title && xScale.title.text) || '' }) || undefined,
                    min: (xScale.min !== undefined) ? xScale.min : undefined,
                    max: (xScale.max !== undefined) ? xScale.max : undefined
                },
                y: {
                    title: (yScale.title && { display: !!(yScale.title && yScale.title.display), text: (yScale.title && yScale.title.text) || '' }) || undefined,
                    beginAtZero: !!yScale.beginAtZero
                }
            }
        };

        const html = `
            <!doctype html>
            <html>
            <head>
            <meta charset="utf-8" />
            <title>Gráfico de Frecuencia (Expandido)</title>
            <style>
              html,body { height:100%; margin:0; background:#f6f7fb; font-family: Inter, sans-serif; }
              .chart-popup-container { width: 100vw; height: 100vh; display:flex; align-items:center; justify-content:center; padding:16px; box-sizing:border-box; }
              canvas { width:100%; height:100%; }
            </style>
            </head>
            <body>
              <div class="chart-popup-container"><canvas id="popupFrequencyChart"></canvas></div>
              <script src="${chartJsUrl}"></script>
              <script>
                (function() {
                  const chartData = ${JSON.stringify(dataCopy)};
                  const chartOptions = ${JSON.stringify(optionsCopy)};
                  function tryCreate() {
                    if (window.Chart) {
                      const ctx = document.getElementById('popupFrequencyChart').getContext('2d');
                      new Chart(ctx, { type: 'line', data: chartData, options: chartOptions });
                    } else { setTimeout(tryCreate, 50); }
                  }
                  tryCreate();
                })();
              </script>
            </body>
            </html>`;
        newWindow.document.open();
        newWindow.document.write(html);
        newWindow.document.close();
    });

    // --- ENVÍO DEL FORMULARIO ---
    form.addEventListener('submit', async (event) => {
        event.preventDefault();

        const formData = new FormData(form);
        const sensor = formData.get('sensor');
        const registro = formData.get('registro');
        const inicioValue = formData.get('inicio');
        const finValue = formData.get('fin');
        
        if (!inicioValue || !finValue) {
            alert("Por favor, selecciona una fecha de inicio y fin.");
            return;
        }
        
        const inicioISO = new Date(inicioValue).toISOString();
        const finISO = new Date(finValue).toISOString();

        // URLs de las APIs
        const apiUrl = `/api/data?start=${encodeURIComponent(inicioISO)}&stop=${encodeURIComponent(finISO)}&channel=${encodeURIComponent(registro)}&sensor=${encodeURIComponent(sensor)}`;
        const apiUrl2 = `/api/inclinacion?start=${encodeURIComponent(inicioISO)}&stop=${encodeURIComponent(finISO)}&sensor=${encodeURIComponent(sensor)}`;

        // --- PETICIÓN 1: Datos Crudos y FFT ---
        try {
            const response = await fetch(apiUrl);
            if (!response.ok) throw new Error(`Error del servidor: ${response.statusText}`);
            const data = await response.json();

            // Destruir instancias previas
            if (timeChart) timeChart.destroy();
            if (frequencyChart) frequencyChart.destroy();
            if (temperatureChart) temperatureChart.destroy();
            if (humidityChart) humidityChart.destroy();
            if (pressureChart) pressureChart.destroy();
            
            // Crear Time Chart
            const timeCtx = document.getElementById('timeChart').getContext('2d');
            timeChart = new Chart(timeCtx, {
                type: 'line',
                data: {
                    labels: data.timeSeries.time,
                    datasets: [{
                        label: `Registro de ${registro} en el tiempo`,
                        data: data.timeSeries.data,
                        borderColor: 'rgba(54, 162, 235, 1)',
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        pointRadius: 1,
                        borderWidth: 1.5
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { title: { display: true, text: 'Tiempo' }, type: 'category', ticks: { maxTicksLimit: 10 } },
                        y: { title: { display: true, text: 'Magnitud (g)' } }
                    }
                }
            });

            // Crear Frequency Chart
            const freqCtx = document.getElementById('frequencyChart').getContext('2d');
            frequencyChart = new Chart(freqCtx, {
                type: 'line',
                data: {
                    labels: data.frequencySeries.frequencies,
                    datasets: [{
                        label: 'Magnitud del espectro (FFT)',
                        data: data.frequencySeries.magnitude,
                        borderColor: 'rgba(255, 99, 132, 1)',
                        backgroundColor: 'rgba(255, 99, 132, 0.2)',
                        pointRadius: 1,
                        borderWidth: 1.5
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { title: { display: true, text: 'Frecuencia (Hz)' }, type: 'linear' },
                        y: { title: { display: true, text: 'Magnitud' }, beginAtZero: true }
                    }
                }
            });

            // Crear Temperature Chart
            const tempCtx = document.getElementById('tempChart').getContext('2d');
            temperatureChart = new Chart(tempCtx, {
                type: 'line',
                data: {
                    labels: data.ambSeries.time,
                    datasets: [{
                        label: 'Registro de temperatura en el tiempo.',
                        data: data.ambSeries.temp,
                        borderColor: 'rgba(20, 24, 236, 0.9)',
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        pointRadius: 1,
                        borderWidth: 1.5
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { title: { display: true, text: 'Tiempo' }, type: 'category', ticks: { maxTicksLimit: 10 } },
                        y: { title: { display: true, text: 'Temperatura (C)' }, ticks: { stepSize: 1 } }
                    }
                }
            });

            // Crear Humidity Chart
            const humeCtx = document.getElementById('humeChart').getContext('2d');
            humidityChart = new Chart(humeCtx, {
                type: 'line',
                data: {
                    labels: data.ambSeries.time,
                    datasets: [{
                        label: 'Registro de humedad en el tiempo.',
                        data: data.ambSeries.hum,
                        borderColor: 'rgba(20, 24, 236, 0.9)',
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        pointRadius: 1,
                        borderWidth: 1.5
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { title: { display: true, text: 'Tiempo' }, type: 'category', ticks: { maxTicksLimit: 10 } },
                        y: { title: { display: true, text: 'Humedad (% HR)' }, ticks: { stepSize: 1 } }
                    }
                }
            });

            // Crear Pressure Chart
            const presCtx = document.getElementById('presChart').getContext('2d');
            pressureChart = new Chart(presCtx, {
                type: 'line',
                data: {
                    labels: data.ambSeries.time,
                    datasets: [{
                        label: 'Registro de presión en el tiempo.',
                        data: data.ambSeries.pres,
                        borderColor: 'rgba(20, 24, 236, 0.9)',
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        pointRadius: 1,
                        borderWidth: 1.5
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { title: { display: true, text: 'Tiempo' }, type: 'category', ticks: { maxTicksLimit: 10 } },
                        y: { title: { display: true, text: 'Presión (hPa)' }, ticks: { stepSize: 1 } }
                    }
                }
            });

        } catch (error) {
            console.error('Error al graficar datos base:', error);
        }

        // --- PETICIÓN 2: Inclinación (Pitch/Roll) ---
        try {
            const response = await fetch(apiUrl2);
            if (!response.ok) throw new Error(`Error del servidor: ${response.statusText}`);
            const data = await response.json();

            // Destruir instancias previas (Usando las variables globales)
            if (pitchChart) pitchChart.destroy();
            if (rollChart) rollChart.destroy();
            
            // Crear Pitch Chart
            const pitchCtx = document.getElementById('pitchChart').getContext('2d');
            pitchChart = new Chart(pitchCtx, {
                type: 'line',
                data: {
                    labels: data.labels, // Data directa de Python
                    datasets: [{
                        label: 'Inclinación respecto al eje Y',
                        data: data.pitch, // Data directa de Python
                        borderColor: 'rgba(54, 162, 235, 1)', // AZUL
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        pointRadius: 1,
                        borderWidth: 1.5,
                        fill: true
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { title: { display: true, text: 'Hora' }, ticks: { maxTicksLimit: 10 } },
                        y: { title: { display: true, text: 'Grados (°)' }, beginAtZero: false, ticks: { stepSize: 0.1 } }
                    }
                }
            });

            // Crear Roll Chart
            const rollCtx = document.getElementById('rollChart').getContext('2d');
            rollChart = new Chart(rollCtx, {
                type: 'line',
                data: {
                    labels: data.labels, // Data directa de Python
                    datasets: [{
                        label: 'Inclinación respecto al eje X',
                        data: data.roll, // Data directa de Python
                        borderColor: 'rgba(255, 99, 132, 1)', // ROJO
                        backgroundColor: 'rgba(255, 99, 132, 0.2)',
                        pointRadius: 1,
                        borderWidth: 1.5,
                        fill: true
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { title: { display: true, text: 'Hora' }, ticks: { maxTicksLimit: 10 } },
                        y: { title: { display: true, text: 'Grados (°)' }, beginAtZero: false, ticks: { stepSize: 0.1 } }
                    }
                }
            });

        } catch (error) {
            console.error('Error al graficar inclinación:', error);
            alert('Error al cargar datos de inclinación. Revisa la consola.');
        }
    });
});