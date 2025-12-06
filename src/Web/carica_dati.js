let REFRESH_RATE = 200; // valore di default in millisecondi

async function caricaDati(){
	try{
		const r = await fetch("../dati.json");
		const d = await r.json();

		const H = document.getElementById("humidity");
		const T = document.getElementById("temperature");
		const V = document.getElementById("volume");

		if(H) H.textContent = d.umidita ?? "";
		if(T) T.textContent = d.temperatura ?? "";
		if(V) V.textContent = d.volume ?? "";

		set(H, d.umidita, "humidity");
		set(T, d.temperatura, "temperature");
		set(V, d.volume, "volume");

	}catch(e){
		console.error("Errore lettura dati:", e);
	}
}

// ---- WIFI ----
async function caricaWifi(){
	try{
		const r = await fetch("wifi.json");
		if(!r.ok) return;
		const w = await r.json();
		if(w.ssid) document.getElementById("ssid").value = w.ssid;
		if(w.password) document.getElementById("password").value = w.password;
	}catch(_){}
}

async function salvaWifi(e){
	if(e) e.preventDefault();

	const payload = {
		ssid: document.getElementById("ssid").value || "",
		password: document.getElementById("password").value || ""
	};

	const st = document.getElementById("saveStatus");
	if(st) st.textContent = "Salvataggio...";

	try{
		const r = await fetch("/wifi.json",{
			method:"POST",
			headers:{"Content-Type":"application/json"},
			body:JSON.stringify(payload)
		});
		if(r.ok){
			if(st) st.textContent = "Salvato sul dispositivo";
			return;
		}
	}catch(_){}

	const blob = new Blob(
		[JSON.stringify(payload,null,2)],
		{type:"application/json"}
	);
	const url = URL.createObjectURL(blob);
	const a = document.createElement("a");
	a.href = url;
	a.download = "wifi.json";
	a.click();
	URL.revokeObjectURL(url);

	if(st) st.textContent = "Download creato (wifi.json)";
}

document.addEventListener("DOMContentLoaded",()=>{
	const form = document.getElementById("settingsBox");
	if(form) form.addEventListener("submit", salvaWifi);
	caricaWifi();
	aggiorna();
});

async function leggiRRate(){
	try{
		const rateFile = await fetch("settings.json");
		const rateJson = await rateFile.json();
		// Leggo e converto la refresh rate
		REFRESH_RATE = Number(rateJson.refresh_rate);
		REFRESH_RATE = REFRESH_RATE*1000;
	}catch(e){
		console.error("Errore nella lettura delle impostazioni:",e);
	}
}

async function aggiorna(){
	while(true){
		await caricaDati();
		await new Promise(r=>setTimeout(r,REFRESH_RATE));
	}
}