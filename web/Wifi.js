async function ScanWifi() {
  const Btn = document.getElementById("ScanBtn");
  const Spinner = document.getElementById("ScanSpinner");
  const Status = document.getElementById("ScanStatus");

  Btn.disabled = true;
  Btn.style.opacity = 0.6;
  Status.classList.remove("hidden");
  Spinner.style.display = "block";

  await fetch("/Scan");

  const checkResults = setInterval(async () => {
    try {
      const response = await fetch("/GetScanResults");
      const text = await response.text();

      if (text !== "WAITING" && text !== "NOT_STARTED") {
        Ssid.innerHTML = text;
        clearInterval(checkResults);
        Btn.disabled = false;
        Btn.style.opacity = 1;
        Spinner.style.display = "none";
        Status.classList.add("hidden");
      }
    } catch (err) {
      console.error("Erreur de scan:", err);
      clearInterval(checkResults);
      Btn.disabled = false;
    }
  }, 1000);
}

function SaveWifi(){

  if(Ssid.value === "" || Pass.value === ""){
    alert("⚠️ Veuillez renseigner le SSID et le mot de passe.");
    return; // on stoppe ici
  }

  fetch("/SaveWifi",{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:`Ssid=${Ssid.value}&Pass=${Pass.value}`
  })
  .then(r=>r.text())
  .then(txt=> {
    alert(txt);
    //Une fois que l'utilisateur a cliqué sur OK, on demande le reboot
    fetch("/RestartESP");
  })
  .catch(err => {
      console.error("Erreur lors de la sauvegarde WIFI :", err);
      alert("Erreur de communication avec le serveur.");
  });
 
}