let UserCalibChanged = false;
let CurrentLimits = null;
let LastGlobalStatus = null;

const TempCheckbox = document.getElementById("EnableTempOffset");
const TempInput    = document.getElementById("TempOffset");

const HumCheckbox = document.getElementById("EnableHumOffset");
const HumInput    = document.getElementById("HumOffset");


TempCheckbox.addEventListener("change", () => { 
    UserCalibChanged = true; 
    UpdateStatus(); // update immédiat si nécessaire
});

TempInput.addEventListener("input", () => { 
    UserCalibChanged = true; 
    UpdateStatus(); // update immédiat si nécessaire
});

HumCheckbox.addEventListener("change", () => { 
    UserCalibChanged = true; 
    UpdateStatus(); // update immédiat si nécessaire
});

HumInput.addEventListener("input", () => { 
    UserCalibChanged = true; 
    UpdateStatus(); // update immédiat si nécessaire
});



function UpdateStatus(d) {
    if (d) LastGlobalStatus = d;
    else d = LastGlobalStatus;
    if (!d) return;
        CurrentLimits = d.Limits && d.Limits.SensorOffset;
        applyNumericLimits(TempInput, CurrentLimits && CurrentLimits.Temperature);
        applyNumericLimits(HumInput, CurrentLimits && CurrentLimits.Humidity);

        // Si l'utilisateur n'est pas en train de modifier, mettre à jour les champs
        if (!UserCalibChanged) {
            TempCheckbox.checked = d.SensorConfig.TempOffsetEnabled;
            TempInput.value = d.SensorConfig.TempOffset;

            HumCheckbox.checked = d.SensorConfig.HumOffsetEnabled;
            HumInput.value = d.SensorConfig.HumOffset;
        }

        // Affichage des valeurs du capteur avec offset
        let DisplayTemp = d.Sensor.Temp;
        let DisplayHum  = d.Sensor.Hum;

        if (TempCheckbox.checked) DisplayTemp += parseFloat(TempInput.value) || 0;
        if (HumCheckbox.checked)  DisplayHum  += parseFloat(HumInput.value) || 0;

        document.getElementById("Temp").innerText = DisplayTemp.toFixed(2);
        document.getElementById("Hum").innerText  = DisplayHum.toFixed(2);

        //griser les boutons quand pas utile
        document.getElementById("CancelBtnCalib").disabled = !UserCalibChanged; 
        document.getElementById("SaveBtnCalib").disabled = !UserCalibChanged; 
}


function SaveCapteurCalibration() {
  // Récupération des valeurs de la page
  const TempEnabled = document.getElementById("EnableTempOffset").checked ? 1 : 0;
  const TempInputValue = document.getElementById("TempOffset").value;
  const TempValue = Number(TempInputValue);
  const HumEnabled  = document.getElementById("EnableHumOffset").checked ? 1 : 0;
  const HumInputValue = document.getElementById("HumOffset").value;
  const HumValue = Number(HumInputValue);
  const temperatureLimits = CurrentLimits && CurrentLimits.Temperature;
  const humidityLimits = CurrentLimits && CurrentLimits.Humidity;

  if (TempInputValue.trim() === "" || HumInputValue.trim() === "" ||
      !isWithinNumericLimits(TempValue, temperatureLimits) ||
      !isWithinNumericLimits(HumValue, humidityLimits)) {
    const temperatureRange = temperatureLimits
      ? formatNumericLimits(temperatureLimits)
      : "indisponible";
    const humidityRange = humidityLimits
      ? formatNumericLimits(humidityLimits)
      : "indisponible";
    alert(`Calibration invalide : température ${temperatureRange} °C, humidité ${humidityRange} points.`);
    return;
  }

  SmartCellarWebSocket.request("saveSensorCalibration", {
    tempEnabled: Boolean(TempEnabled),
    tempOffset: TempValue,
    humEnabled: Boolean(HumEnabled),
    humOffset: HumValue
  })
  .then(result => {
    if (!result.persisted) throw new Error("La calibration n'a pas été enregistrée.");
    alert("Calibration enregistrée.");
    UserCalibChanged = false;
    UpdateStatus();
  })
  .catch(err => {
    console.error("Erreur lors de la sauvegarde de la calibration :", err);
    alert(`Échec de la sauvegarde : ${err.message}`);
  });
}

function UndoChangesCalib() {
  UserCalibChanged = false;
  UpdateStatus();
}



SmartCellarWebSocket.onState(UpdateStatus);
