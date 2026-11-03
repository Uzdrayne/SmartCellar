
const wsPulse = document.getElementById("ws-pulse");
const wsStatus = document.getElementById("ws-status");

const LabelTemp         = document.getElementById("LabelTemp") ; 
const LabelConsigneTemp = document.getElementById("LabelConsigneTemp") ; 
const SliderTemp        = document.getElementById("SliderSetTemp");

const LabelHum          = document.getElementById("LabelHum") ;
const LabelConsigneHum  = document.getElementById("LabelConsigneHum") ; 
const SliderHum         = document.getElementById("SliderSetHum");

const SaveBtnConsigne   = document.getElementById("SaveBtnConsigne");
const CancelBtnConsigne = document.getElementById("CancelBtnConsigne");

const CheckStatusCold   = document.getElementById("CheckStatusCold");
const CheckStatusHeat   = document.getElementById("CheckStatusHeat");
const CheckStatusDehum  = document.getElementById("CheckStatusDehum");
const CheckStatusHum    = document.getElementById("CheckStatusHum");
const CheckStatusFan    = document.getElementById("CheckStatusFan");




//SGV - TEMP

// Température actuelle
const SvgLineCurrentTemp = document.getElementById("SvgLineCurrentTemp");
const SvgLineHeatLow = document.getElementById("SvgLineHeatLow");
const SvgLineHeatHigh = document.getElementById("SvgLineHeatHigh");
const SvgLineColdLow = document.getElementById("SvgLineColdLow");
const SvgLineColdHigh = document.getElementById("SvgLineColdHigh");
const SvgLabelCurTemp = document.getElementById("SvgLabelCurTemp");
const SvgLabelCurTempValue = document.getElementById("SvgLabelCurTempValue");
const TempAxisMin = document.getElementById("TempAxisMin");
const TempAxisMax = document.getElementById("TempAxisMax");

const SvgZoneTempDead= document.getElementById("SvgZoneTempDead");

// Consigne 
const SvgLineSetTemp = document.getElementById("SvgLineSetTemp");
const SVGLabelSetTemp = document.getElementById("SVGLabelSetTemp");
const SVGLabelSetTempValue = document.getElementById("SVGLabelSetTempValue");

// HEAT 
const SvgZoneHeatRect = document.getElementById("SvgZoneHeatRect");
const SvgZoneHeatRectExtend = document.getElementById("SvgZoneHeatRectExtend");
const SvgLabelHeatLow  = document.getElementById("SvgLabelHeatLow");
const SvgLabelHeatLowValue  = document.getElementById("SvgLabelHeatLowValue");
const SvgLabelHeatHigh  = document.getElementById("SvgLabelHeatHigh");
const SvgLabelHeatHighValue  = document.getElementById("SvgLabelHeatHighValue");

// COLD 
const SvgZoneColdRect = document.getElementById("SvgZoneColdRect");
const SvgZoneColdRectExtend = document.getElementById("SvgZoneColdRectExtend");
const SvgLabelColdLow = document.getElementById("SvgLabelColdLow");
const SvgLabelColdLowValue = document.getElementById("SvgLabelColdLowValue");
const SvgLabelColdHigh = document.getElementById("SvgLabelColdHigh");
const SvgLabelColdHighValue = document.getElementById("SvgLabelColdHighValue");
const SvgArrowColdLow = document.getElementById("SvgArrowColdLow");
const SvgArrowColdHigh = document.getElementById("SvgArrowColdHigh");
const SvgArrowHeatLow = document.getElementById("SvgArrowHeatLow");
const SvgArrowHeatHigh = document.getElementById("SvgArrowHeatHigh");

//MSG
const SvgLabelMsgHeat = document.getElementById("SvgLabelMsgHeat");
const SvgLabelMsgCold = document.getElementById("SvgLabelMsgCold");




//SGV - HYGRO

// Huimidité actuelle
const SvgLineCurrentHum = document.getElementById("SvgLineCurrentHum");
const SvgLineHumLow = document.getElementById("SvgLineHumLow");
const SvgLineHumHigh = document.getElementById("SvgLineHumHigh");
const SvgLineDehumLow = document.getElementById("SvgLineDehumLow");
const SvgLineDehumHigh = document.getElementById("SvgLineDehumHigh");
const SvgLabelCurHum = document.getElementById("SvgLabelCurHum");
const SvgLabelCurHumValue = document.getElementById("SvgLabelCurHumValue");
const HumAxisMin = document.getElementById("HumAxisMin");
const HumAxisMax = document.getElementById("HumAxisMax");

const FanCycleTrack = document.getElementById("FanCycleTrack");
const FanOnSegment = document.getElementById("FanOnSegment");
const FanOffSegment = document.getElementById("FanOffSegment");
const FanCycleMarker = document.getElementById("FanCycleMarker");
const FanCycleState = document.getElementById("FanCycleState");
const FanCycleRemaining = document.getElementById("FanCycleRemaining");
const FanChartOnDuration = document.getElementById("FanChartOnDuration");
const FanChartOffDuration = document.getElementById("FanChartOffDuration");

const SvgZoneHumDead= document.getElementById("SvgZoneHumDead");

// Consigne 
const SvgLineSetHum = document.getElementById("SvgLineSetHum");
const SVGLabelSetHum = document.getElementById("SVGLabelSetHum");
const SVGLabelSetHumValue = document.getElementById("SVGLabelSetHumValue");

// Hum 
const SvgZoneHumRect = document.getElementById("SvgZoneHumRect");
const SvgZoneHumRectExtend = document.getElementById("SvgZoneHumRectExtend");
const SvgLabelHumLow  = document.getElementById("SvgLabelHumLow");
const SvgLabelHumLowValue  = document.getElementById("SvgLabelHumLowValue");
const SvgLabelHumHigh  = document.getElementById("SvgLabelHumHigh");
const SvgLabelHumHighValue  = document.getElementById("SvgLabelHumHighValue");

// Dehum 
const SvgZoneDehumRect = document.getElementById("SvgZoneDehumRect");
const SvgZoneDehumRectExtend = document.getElementById("SvgZoneDehumRectExtend");
const SvgLabelDehumLow = document.getElementById("SvgLabelDehumLow");
const SvgLabelDehumLowValue = document.getElementById("SvgLabelDehumLowValue");
const SvgLabelDehumHigh = document.getElementById("SvgLabelDehumHigh");
const SvgLabelDehumHighValue = document.getElementById("SvgLabelDehumHighValue");
const SvgArrowDehumLow = document.getElementById("SvgArrowDehumLow");
const SvgArrowDehumHigh = document.getElementById("SvgArrowDehumHigh");
const SvgArrowHumLow = document.getElementById("SvgArrowHumLow");
const SvgArrowHumHigh = document.getElementById("SvgArrowHumHigh");

//MSG
const SvgLabelMsgHum = document.getElementById("SvgLabelMsgHum");
const SvgLabelMsgDehum = document.getElementById("SvgLabelMsgDehum");


//si false : capteur HS (NaN)
let tempOk =false;
let humOk =false;

//temp et hum corrigé
let ValTempCorr = 0;
let ValHumCorr = 0;


let LastHardStatus = null;

let UserChanged = false;
let consigneSaveInProgress = false;

function updateConsigneButtons() {
  const disabled = !UserChanged || consigneSaveInProgress;
  SaveBtnConsigne.disabled = disabled;
  CancelBtnConsigne.disabled = disabled;
}

let wsWatchdog; // Timer pour détecter la perte de données


SliderTemp.addEventListener("input", () => { 
  LabelConsigneTemp.innerText = SliderTemp.value; 
  UserChanged = true;
  updateConsigneButtons();
});

SliderHum.addEventListener("input", () => { 
  LabelConsigneHum.innerText = SliderHum.value; 
  UserChanged = true;
  updateConsigneButtons();
});






// --- LOGIQUE WEBSOCKET ---
let gateway = `ws://${window.location.hostname}/ws`;
let websocket;
let wsReconnectTimer = null;
let wsReconnectDelayMs = 2000;
let wsConnectionStableTimer = null;

function initWebSocket() {
    if (websocket && websocket.readyState !== WebSocket.CLOSED) {
        return;
    }

    console.log('Tentative de connexion WebSocket...');
    const socket = new WebSocket(gateway);
    websocket = socket;
    socket.onopen = event => onOpen(event, socket);
    socket.onclose = event => onClose(event, socket);
    socket.onmessage = event => onMessage(event, socket);
}

function onOpen(event, socket) {
  if (socket !== websocket) return;
  console.log('Connexion établie');
  wsPulse.style.backgroundColor = "#4caf50"; // Vert
  wsPulse.style.animation = "pulse 1.5s infinite"; // Petit effet optionnel
  wsStatus.innerText = "Connecté";

  clearTimeout(wsConnectionStableTimer);
  wsConnectionStableTimer = setTimeout(() => {
    if (socket === websocket) {
      wsReconnectDelayMs = 2000;
    }
  }, 10000);
}

function onClose(event, socket) {
    if (socket !== websocket) return;
    console.log('Connexion perdue');
    clearTimeout(wsWatchdog);
    wsWatchdog = null;
    clearTimeout(wsConnectionStableTimer);
    websocket = null;
    
    wsPulse.style.backgroundColor = "#f44336"; // Rouge
    wsPulse.style.animation = "none";
    wsStatus.innerText = "Déconnecté";
    
    if (wsReconnectTimer === null) {
        const reconnectDelay = wsReconnectDelayMs;
        wsReconnectDelayMs = Math.min(wsReconnectDelayMs * 2, 30000);
        wsReconnectTimer = setTimeout(() => {
            wsReconnectTimer = null;
            initWebSocket();
        }, reconnectDelay);
    }
}



function onMessage(event, socket) {
    if (socket !== websocket) return;
// 1. On annule le précédent timer de sécurité
    clearTimeout(wsWatchdog);

    // 2. On lance un nouveau timer de 3 secondes
    // Si rien n'arrive pendant 3s, on considère que c'est déconnecté
    wsWatchdog = setTimeout(() => {
        if (socket !== websocket || socket.readyState !== WebSocket.OPEN) return;
        console.log("Watchdog: Aucune donnée reçue depuis 3s");
        socket.close();
    }, 3000);

    // 3. Traitement normal des données
    const d = JSON.parse(event.data);
    processData(d);
}


function isValidNumber(v) {
    return (
        v !== null && 
        v !== undefined &&
        typeof v === "number" && 
        Number.isFinite(v) && // Écarte NaN et Infinity
        v !== -127       // Valeur d'erreur classique de certaines sondes
    );
}


// --- TRAITEMENT DES DONNÉES ---
function processData(d) {

    let ValTemp = d.Sensor.Temp;
    let ValHum = d.Sensor.Hum;

    tempOk = isValidNumber(ValTemp);
    humOk  = isValidNumber(ValHum);

    // Valeur brute du capteur
    ValTempCorr = ValTemp
    ValHumCorr  = ValHum
    // Appliquer les offsets
    if (d.SensorConfig.TempOffsetEnabled) ValTempCorr += d.SensorConfig.TempOffset;
    if (d.SensorConfig.HumOffsetEnabled)  ValHumCorr += d.SensorConfig.HumOffset;



    // Affichage des valeurs en temps réel
    if(tempOk){
      LabelTemp.innerText = ValTempCorr.toFixed(2);
    }else{
      LabelTemp.innerText = "erreur"
    }
        
    if(humOk){
      LabelHum.innerText  = ValHumCorr.toFixed(2);
    }else{
      LabelHum.innerText = "erreur"
    }
    

    // Mise à jour des sliders uniquement si l'utilisateur ne touche à rien
    if(!UserChanged){
        LabelConsigneTemp.innerText = d.Consigne.SetTemp.toFixed(2);
        SliderTemp.value = d.Consigne.SetTemp;
        LabelConsigneHum.innerText = d.Consigne.SetHum.toFixed(2);
        SliderHum.value = d.Consigne.SetHum;
    }

    // État des boutons
    updateConsigneButtons();

    // État des switchs relais
    CheckStatusCold.checked = d.State.StateCold;
    CheckStatusHeat.checked = d.State.StateHeat;
    CheckStatusDehum.checked = d.State.StateDehum;
    CheckStatusHum.checked = d.State.StateHum;
    CheckStatusFan.checked = d.Fan.State;

    // Mise à jour des graphiques SVG
    updateTempSvg(d);
    updateHumSvg(d);
    updateFanCycleSvg(d.Fan);
}

// Au chargement de la page
window.addEventListener('load', initWebSocket);




function SaveSettings() {
    if (!UserChanged || consigneSaveInProgress) {
        return;
    }

    const newTemp = SliderTemp.value;
    const newHum = SliderHum.value;
    consigneSaveInProgress = true;
    updateConsigneButtons();

    fetch("/SaveConsignes", {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `Temp=${newTemp}&Hum=${newHum}`
    })
    .then(async response => {
        const text = await response.text();
        if (!response.ok) {
            throw new Error(text || `HTTP ${response.status}`);
        }
        return text;
    })
    .then(text => {
        UserChanged = false;
        consigneSaveInProgress = false;
        updateConsigneButtons();
        alert(text);
    })
    .catch(error => {
        consigneSaveInProgress = false;
        updateConsigneButtons();
        console.error("Erreur lors de la sauvegarde consignes :", error);
        alert(`Échec de la sauvegarde : ${error.message}`);
    });
}
function UndoChanges() {
  if (consigneSaveInProgress) return;
  UserChanged = false;
  updateConsigneButtons();
}


const ChartLeft = 70;
const ChartRight = 930;

function getChartScale(values) {
  const validValues = values.filter(Number.isFinite);
  const controlMin = Math.min(...validValues);
  const controlMax = Math.max(...validValues);
  const padding = Math.max(1, (controlMax - controlMin) * 0.5);
  return { min: controlMin - padding, max: controlMax + padding };
}

function mapVal(value, scale) {
  const ratio = (value - scale.min) / (scale.max - scale.min);
  return ChartLeft + Math.max(0, Math.min(1, ratio)) * (ChartRight - ChartLeft);
}

function setLinePosition(line, x) {
  line.setAttribute("x1", x);
  line.setAttribute("x2", x);
}

function setZone(rect, firstValue, secondValue, scale) {
  const firstX = mapVal(firstValue, scale);
  const secondX = mapVal(secondValue, scale);
  const left = Math.min(firstX, secondX);
  const width = Math.max(0, Math.abs(secondX - firstX));
  rect.setAttribute("x", left);
  rect.setAttribute("width", width);
  rect.style.display = width > 0 ? "block" : "none";
}

function setChartLabel(label, value, x, labelText, valueText) {
  const safeX = Math.max(ChartLeft, Math.min(ChartRight, x));
  const anchor = safeX <= ChartLeft + 20 ? "start" :
    safeX >= ChartRight - 20 ? "end" : "middle";
  label.setAttribute("x", safeX);
  value.setAttribute("x", safeX);
  label.style.textAnchor = anchor;
  value.style.textAnchor = anchor;
  label.textContent = labelText;
  value.textContent = valueText;
}

function setDirectionArrow(arrow, label, direction) {
  const labelX = Number(label.getAttribute("x"));
  const labelWidth = label.getComputedTextLength();
  const anchor = label.style.textAnchor;
  const labelLeft = anchor === "start" ? labelX
    : anchor === "end" ? labelX - labelWidth
      : labelX - labelWidth / 2;
  const labelRight = labelLeft + labelWidth;
  const labelY = Number(label.getAttribute("y")) + 4;
  const arrowLength = 28;
  const gap = 5;

  if (direction === "left") {
    arrow.setAttribute("x1", labelLeft - gap);
    arrow.setAttribute("x2", labelLeft - gap - arrowLength);
  } else {
    arrow.setAttribute("x1", labelRight + gap);
    arrow.setAttribute("x2", labelRight + gap + arrowLength);
  }
  arrow.setAttribute("y1", labelY);
  arrow.setAttribute("y2", labelY);
  arrow.style.display = "block";
}

function setAxisLabels(minLabel, maxLabel, scale, unit) {
  minLabel.textContent = `${scale.min.toFixed(1)}${unit}`;
  minLabel.setAttribute("x", ChartLeft);
  minLabel.style.textAnchor = "start";
  maxLabel.textContent = `${scale.max.toFixed(1)}${unit}`;
  maxLabel.setAttribute("x", ChartRight);
  maxLabel.style.textAnchor = "end";
}

function updateCurrentMarker(line, label, value, measurement, valid, scale, unit, noun) {
  if (!valid || !Number.isFinite(measurement)) {
    line.style.display = "none";
    setChartLabel(label, value, (ChartLeft + ChartRight) / 2,
      `${noun} actuelle`, "Capteur indisponible");
    return;
  }

  line.style.display = "block";
  const x = mapVal(measurement, scale);
  setLinePosition(line, x);
  const outsideLow = measurement < scale.min;
  const outsideHigh = measurement > scale.max;
  const labelText = outsideLow
    ? `${noun} sous l'échelle`
    : outsideHigh ? `${noun} au-dessus de l'échelle` : `${noun} actuelle`;
  const valueText = `${measurement.toFixed(2)}${unit}${outsideLow || outsideHigh ? " · hors échelle" : ""}`;
  setChartLabel(label, value, x, labelText, valueText);
}

function updateTempSvg(Status) {
  const tempCur = ValTempCorr;
  const tempSet = Number(SliderTemp.value);
  const heatOn = tempSet + Number(Status.HardwareHeat.HysLow);
  const heatOff = tempSet + Number(Status.HardwareHeat.HysHigh);
  const coldOff = tempSet + Number(Status.HardwareCold.HysLow);
  const coldOn = tempSet + Number(Status.HardwareCold.HysHigh);
  const scale = getChartScale([tempSet, heatOn, heatOff, coldOff, coldOn]);

  const xSet = mapVal(tempSet, scale);
  const xHeatOn = mapVal(heatOn, scale);
  const xHeatOff = mapVal(heatOff, scale);
  const xColdOff = mapVal(coldOff, scale);
  const xColdOn = mapVal(coldOn, scale);

  setLinePosition(SvgLineSetTemp, xSet);
  setLinePosition(SvgLineHeatLow, xHeatOn);
  setLinePosition(SvgLineHeatHigh, xHeatOff);
  setLinePosition(SvgLineColdLow, xColdOff);
  setLinePosition(SvgLineColdHigh, xColdOn);

  setZone(SvgZoneHeatRectExtend, scale.min, heatOn, scale);
  setZone(SvgZoneHeatRect, heatOn, heatOff, scale);
  setZone(SvgZoneColdRect, coldOff, coldOn, scale);
  setZone(SvgZoneColdRectExtend, coldOn, scale.max, scale);

  if (heatOff < coldOff) {
    setZone(SvgZoneTempDead, heatOff, coldOff, scale);
  } else {
    SvgZoneTempDead.style.display = "none";
  }

  setChartLabel(SVGLabelSetTemp, SVGLabelSetTempValue, xSet,
    "Consigne", `${tempSet.toFixed(2)} °C`);
  setChartLabel(SvgLabelHeatLow, SvgLabelHeatLowValue, xHeatOn,
    "Chaud ON", `${heatOn.toFixed(2)} °C`);
  setChartLabel(SvgLabelHeatHigh, SvgLabelHeatHighValue, xHeatOff,
    "Chaud OFF", `${heatOff.toFixed(2)} °C`);
  setChartLabel(SvgLabelColdLow, SvgLabelColdLowValue, xColdOff,
    "Froid OFF", `${coldOff.toFixed(2)} °C`);
  setChartLabel(SvgLabelColdHigh, SvgLabelColdHighValue, xColdOn,
    "Froid ON", `${coldOn.toFixed(2)} °C`);
  setDirectionArrow(SvgArrowHeatLow, SvgLabelHeatLow, "left");
  setDirectionArrow(SvgArrowHeatHigh, SvgLabelHeatHigh, "right");
  setDirectionArrow(SvgArrowColdLow, SvgLabelColdLow, "left");
  setDirectionArrow(SvgArrowColdHigh, SvgLabelColdHigh, "right");
  setAxisLabels(TempAxisMin, TempAxisMax, scale, " °C");
  updateCurrentMarker(SvgLineCurrentTemp, SvgLabelCurTemp, SvgLabelCurTempValue,
    tempCur, tempOk, scale, " °C", "Température");

  SvgLabelMsgCold.textContent = Status.DebugMsg.ColdDebugMsg;
  SvgLabelMsgHeat.textContent = Status.DebugMsg.HeatDebugMsg;
}

function updateHumSvg(Status) {
  const humidity = ValHumCorr;
  const humSet = Number(SliderHum.value);
  const humidifierOn = humSet + Number(Status.HardwareHum.HysLow);
  const humidifierOff = humSet + Number(Status.HardwareHum.HysHigh);
  const dehumidifierOff = humSet + Number(Status.HardwareDehum.HysLow);
  const dehumidifierOn = humSet + Number(Status.HardwareDehum.HysHigh);
  const scale = getChartScale([
    humSet, humidifierOn, humidifierOff, dehumidifierOff, dehumidifierOn
  ]);

  const xSet = mapVal(humSet, scale);
  const xHumOn = mapVal(humidifierOn, scale);
  const xHumOff = mapVal(humidifierOff, scale);
  const xDehumOff = mapVal(dehumidifierOff, scale);
  const xDehumOn = mapVal(dehumidifierOn, scale);

  setLinePosition(SvgLineSetHum, xSet);
  setLinePosition(SvgLineHumLow, xHumOn);
  setLinePosition(SvgLineHumHigh, xHumOff);
  setLinePosition(SvgLineDehumLow, xDehumOff);
  setLinePosition(SvgLineDehumHigh, xDehumOn);

  setZone(SvgZoneHumRectExtend, scale.min, humidifierOn, scale);
  setZone(SvgZoneHumRect, humidifierOn, humidifierOff, scale);
  setZone(SvgZoneDehumRect, dehumidifierOff, dehumidifierOn, scale);
  setZone(SvgZoneDehumRectExtend, dehumidifierOn, scale.max, scale);

  if (humidifierOff < dehumidifierOff) {
    setZone(SvgZoneHumDead, humidifierOff, dehumidifierOff, scale);
  } else {
    SvgZoneHumDead.style.display = "none";
  }

  setChartLabel(SVGLabelSetHum, SVGLabelSetHumValue, xSet,
    "Consigne", `${humSet.toFixed(2)} %`);
  setChartLabel(SvgLabelHumLow, SvgLabelHumLowValue, xHumOn,
    "Humid. ON", `${humidifierOn.toFixed(2)} %`);
  setChartLabel(SvgLabelHumHigh, SvgLabelHumHighValue, xHumOff,
    "Humid. OFF", `${humidifierOff.toFixed(2)} %`);
  setChartLabel(SvgLabelDehumLow, SvgLabelDehumLowValue, xDehumOff,
    "Déshum. OFF", `${dehumidifierOff.toFixed(2)} %`);
  setChartLabel(SvgLabelDehumHigh, SvgLabelDehumHighValue, xDehumOn,
    "Déshum. ON", `${dehumidifierOn.toFixed(2)} %`);
  setDirectionArrow(SvgArrowHumLow, SvgLabelHumLow, "left");
  setDirectionArrow(SvgArrowHumHigh, SvgLabelHumHigh, "right");
  setDirectionArrow(SvgArrowDehumLow, SvgLabelDehumLow, "left");
  setDirectionArrow(SvgArrowDehumHigh, SvgLabelDehumHigh, "right");
  setAxisLabels(HumAxisMin, HumAxisMax, scale, " %");
  updateCurrentMarker(SvgLineCurrentHum, SvgLabelCurHum, SvgLabelCurHumValue,
    humidity, humOk, scale, " %", "Humidité");

  SvgLabelMsgDehum.textContent = Status.DebugMsg.DehumDebugMsg;
  SvgLabelMsgHum.textContent = Status.DebugMsg.HumDebugMsg;
}

function formatCycleTime(seconds) {
  const safeSeconds = Math.max(0, Math.floor(seconds));
  const minutes = Math.floor(safeSeconds / 60);
  const remainder = String(safeSeconds % 60).padStart(2, "0");
  return `${minutes}:${remainder}`;
}

function updateFanCycleSvg(fan) {
  if (!fan || typeof fan.Enabled !== "boolean" ||
      typeof fan.State !== "boolean") {
    FanCycleState.textContent = "Données indisponibles";
    FanCycleRemaining.textContent = "Cycle de ventilation non renseigné.";
    FanCycleMarker.style.display = "none";
    return;
  }

  const onMinutes = Number(fan.OnMinutes);
  const offMinutes = Number(fan.OffMinutes);
  const totalMinutes = onMinutes + offMinutes;
  if (!Number.isFinite(totalMinutes) || totalMinutes <= 0) {
    FanCycleState.textContent = "Configuration invalide";
    FanCycleRemaining.textContent = "Vérifiez les durées de ventilation.";
    FanCycleMarker.style.display = "none";
    return;
  }

  const onPercent = (onMinutes / totalMinutes) * 100;
  FanOnSegment.style.width = `${onPercent}%`;
  FanOffSegment.style.left = `${onPercent}%`;
  FanOffSegment.style.width = `${100 - onPercent}%`;
  FanChartOnDuration.textContent = `${onMinutes} min`;
  FanChartOffDuration.textContent = `${offMinutes} min`;

  FanCycleState.className = "fan-state-pill";
  if (!fan.Enabled) {
    FanCycleState.textContent = "Désactivée";
    FanCycleState.classList.add("is-disabled");
    FanCycleRemaining.textContent = "Le cycle est suspendu; les durées sont conservées.";
    FanCycleMarker.style.display = "none";
    FanCycleTrack.setAttribute("aria-valuenow", "0");
    FanCycleTrack.setAttribute("aria-valuetext", "Ventilation désactivée");
    FanOnSegment.style.opacity = "0.35";
    FanOffSegment.style.opacity = "0.35";
    return;
  }

  FanOnSegment.style.opacity = "1";
  FanOffSegment.style.opacity = "1";
  const elapsed = Number(fan.ElapsedSeconds);
  const duration = Number(fan.DurationSeconds);
  const phaseProgress = Number.isFinite(duration) && duration > 0
    ? Math.max(0, Math.min(1, elapsed / duration))
    : 0;
  const position = fan.State
    ? phaseProgress * onPercent
    : onPercent + phaseProgress * (100 - onPercent);
  const remaining = Math.max(0, duration - elapsed);

  FanCycleMarker.style.display = "block";
  FanCycleMarker.style.left = `${position}%`;
  FanCycleTrack.setAttribute("aria-valuenow", String(Math.round(position)));
  FanCycleTrack.setAttribute("aria-valuetext", `Position ${Math.round(position)} pour cent`);
  FanCycleState.textContent = fan.State ? "Ventilation ON" : "Ventilation OFF";
  FanCycleState.classList.add(fan.State ? "is-on" : "is-off");
  FanCycleRemaining.textContent = duration > 0
    ? `Phase ${fan.State ? "ON" : "OFF"} · ${formatCycleTime(elapsed)} / ${formatCycleTime(duration)} min · reste ${formatCycleTime(remaining)} min`
    : "En attente du démarrage du cycle.";
}
