// State elements
const ventana = document.getElementById('ventana');
const ventanaVal = document.getElementById('ventanaVal');
const umbral = document.getElementById('umbral');
const umbralVal = document.getElementById('umbralVal');
const frecuencia = document.getElementById('frecuencia');
const lora = document.getElementById('lora');
const device = document.getElementById('device');

const syncModal = document.getElementById('syncModal');
const cancelSyncBtn = document.getElementById('cancelSync');

const configurarBtn = document.querySelector('button[data-command="configurar"]');
const sincronizarBtn = document.querySelector('button[data-command="sync"]');
const detectarBtn = document.querySelector('button[data-command="detectar"]');

const btnSyncSimple = document.querySelector('button[data-command="sync_simple"]');
const btnSyncCompleta = document.querySelector('button[data-command="sync_completa"]');

// Monitor elements
const monitorFeed = document.getElementById('monitorFeed');
const monitorWrapper = document.getElementById('monitorWrapper');

// Modes
const modesList = document.getElementById('modesList');
const modoActivoEl = document.getElementById('modoActivo');

// Initialize display
ventanaVal.textContent = ventana.value;
umbralVal.textContent = Number(umbral.value).toFixed(2);

ventana.addEventListener('input', () => ventanaVal.textContent = ventana.value);
umbral.addEventListener('input', () => umbralVal.textContent = Number(umbral.value).toFixed(2));

// Modes click handling
modesList.addEventListener('click', async (ev) => {
  const btn = ev.target.closest('button');
  if (!btn) return;
  // deactivate all
  Array.from(modesList.querySelectorAll('button')).forEach(b => {
    b.classList.remove('active');
    b.classList.add('inactive');
  });
  btn.classList.add('active'); btn.classList.remove('inactive');
  //modoActivoEl.textContent = btn.dataset.command;

  const command = btn.dataset.command;
  let dataToSend = {device: device.value};

  monitorFeed.innerHTML = '';

  await sendCommand(command, dataToSend);

  if (command === 'eventos') {
    listenToStream(); // Si es 'eventos', empieza a escuchar
  }
  else if (command === 'tiempo') {
    listenToStream();
  }
  else if (command === 'standby') {
    listenToStream();
  }
  else {
    if (eventSource) { // Para cualquier otro comando, cierra la conexión
      eventSource.close();
    }
  }
});

function guardarConfiguracion() {
  const configuracionObjeto = {
    ventana: ventana.value,
    umbral: umbral.value,
    frecuencia: frecuencia.value,
    lora: lora.value,
    device: device.value
  };
  sendCommand('configurar', configuracionObjeto);
  monitorFeed.innerHTML = '';
  listenToStream();
}
function ejecutarSincronizacion(tipoSync) {
  const configuracionObjeto = {
    device: device.value,
    tipo: tipoSync // Se enviará 'simple' o 'completa' a Flask
  };
  
  sendCommand('sync', configuracionObjeto);

  monitorFeed.innerHTML = '';
  listenToStream();
  
  // Ocultar modal al terminar
  syncModal.classList.add('hidden');
}
function detectarSensores() {
  sendCommand('detectar');

  monitorFeed.innerHTML = '';
  listenToStream();
}

configurarBtn.addEventListener('click', guardarConfiguracion);
detectarBtn.addEventListener('click', detectarSensores);
btnSyncSimple.addEventListener('click', () => ejecutarSincronizacion('simple'));
btnSyncCompleta.addEventListener('click', () => ejecutarSincronizacion('completa'));

sincronizarBtn.addEventListener('click', () => {
  syncModal.classList.remove('hidden');
});

cancelSyncBtn.addEventListener('click', () => {
  syncModal.classList.add('hidden');
});


const MAX_LOGS = 200;
let eventSource = null;

function addLog(text){
  const item = document.createElement('div');
  item.className = 'log-item';
  item.textContent = `${new Date().toLocaleTimeString()} — ${text}`;

  monitorFeed.appendChild(item);
  monitorWrapper.scrollTop = monitorWrapper.scrollHeight;

  while (monitorFeed.children.length > MAX_LOGS) {
    monitorFeed.removeChild(monitorFeed.firstChild);
  }
}

const listenToStream = () => {
    // Cierra cualquier conexión anterior para evitar duplicados
    if (eventSource) {
        eventSource.close();
    }
    // Crea la nueva conexión al endpoint del stream en Flask
    eventSource = new EventSource('/api/stream');
    
    // Se ejecuta cada vez que el servidor envía un mensaje 'data:'
    eventSource.onmessage = function(event) {
        // Añade la nueva línea de texto que llega del servidor
        addLog(event.data);
    };
    // Se ejecuta si hay un error (ej. el servidor cierra la conexión)
    eventSource.onerror = function() {
        addLog("Finalizada la conexión.", true);
        eventSource.close();
    };
};

const sendCommand = async (command, data = {}) => {

    configurarBtn.disabled = true;
    modesList.querySelectorAll('button').forEach(b => b.disabled = true);

    try {
        const response = await fetch(`/api/command/${command}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data),
        });
        if (!response.ok) throw new Error(`Error del servidor: ${response.statusText}`);
        const result = await response.json();
        addLog(result.message);
    } catch (error) {
        console.error('Error al enviar el comando:', error);
        addLog(`Error: ${error.message}`, true);
    }
    finally {
      configurarBtn.disabled = false;
      modesList.querySelectorAll('button').forEach(b => b.disabled = false);
    }
};

function setLedColor(ledIndex, mode) {
  const led = document.getElementById(`led-${ledIndex}`);
  let color = 'OFF';
  
  if (!led) {
    console.error(`El LED ${ledIndex} no existe en el DOM.`);
    return;
  }

  if (mode === 'STANDBY') {
    color = 'blue';
  }
  else if (mode === 'EVENTO') {
    color = 'green';
  }
  else if (mode === 'TIEMPO') {
    color = 'red';
  }
  else if (mode === 'CLOCK') {
    color = 'yellow';
  }

  // Limpiar cualquier estado de color anterior
  led.classList.remove('yellow', 'green', 'blue', 'red');

  // Añadir la nueva clase de color si no se desea apagar ('off')
  if (color !== 'OFF') {
    led.classList.add(color);
  }
};

function actualizarSensores() {
  fetch('/api/detectar', {
      method: 'POST',
      headers: {
          'Content-Type': 'application/json'
      }
  })
  .then(response => {
      if (!response.ok) {
          throw new Error('Error en la comunicación con el servidor');
      }
      return response.json(); 
  })
  .then(data => {
      // Iteramos sobre cada sensor inteligente en el JSON recibido
      for (const [ledIndex, mode] of Object.entries(data)) {
          setLedColor(ledIndex, mode); 
      }
  })
  .catch(error => {
      console.error('Hubo un problema actualizando los indicadores:', error);
  });
}

// 2. Ejecutamos la función inmediatamente al cargar la página
actualizarSensores();

listenToStream();

// 3. Programamos la ejecución continua cada 1.000 milisegundos (1 segundo)
const sensorInterval = setInterval(actualizarSensores, 1000);


window.webappAPI = {
  pushMonitor(text){ addLog(text); }
};

window.addEventListener('beforeunload', () => clearInterval(sensorInterval));
