let UserCalibChanged = false;

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



function UpdateStatus() {
    fetch("/GlobalStatus").then(r => r.json()).then(d => {

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
    });
}


function SaveCapteurCalibration() {
  // Récupération des valeurs de la page
  const TempEnabled = document.getElementById("EnableTempOffset").checked ? 1 : 0;
  const TempValue   = parseFloat(document.getElementById("TempOffset").value) || 0;
  const HumEnabled  = document.getElementById("EnableHumOffset").checked ? 1 : 0;
  const HumValue    = parseFloat(document.getElementById("HumOffset").value) || 0;

  // Préparation des données pour POST
  const bodyData = `TempEnabled=${TempEnabled}&TempValue=${TempValue}&HumEnabled=${HumEnabled}&HumValue=${HumValue}`;

  // Envoi vers le serveur en POST
  fetch("/SaveCapteurCalibration", {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: bodyData
  })
  .then(r => r.text())
  .then(txt => {
    alert(txt);

    // Mise à jour graphique
    UserCalibChanged = false;
    UpdateStatus();
  })
  .catch(err => console.error("Erreur lors de la sauvegarde de la calibration :", err));
}

function UndoChangesCalib() {
  UserCalibChanged = false;
  UpdateStatus();
}



setInterval(UpdateStatus,1000);
UpdateStatus();

