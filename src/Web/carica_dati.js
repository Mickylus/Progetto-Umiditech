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
async function caricaImpostazioni(){
	try{
		const r = await fetch("settings.json");
		const w = await r.json();
		document.getElementById("ssid").value = w.ssid;
		document.getElementById("password").value = w.password;
		document.getElementById("r_rate").value =w.refresh_rate;
		REFRESH_RATE=Number(w.refresh_rate)*1000;
	}catch(_){}
}

async function salvaWifi(e){
	if(e){
		e.preventDefault();
	}
	const ssid =  document.getElementById("ssid").value || "";
	const password = document.getElementById("password").value || "";
	const refresh_rate = document.getElementById("r_rate").value || "";
	const cb = document.getElementById("setDefaultWifi");
	
	const r = await fetch("settings.json");
	settings = await r.json();
	
	settings.ssid = ssid;
    settings.password = password;
    if(cb && cb.checked){
        settings.d_ssid = ssid;
        settings.d_password = password;
    }
	settings.refresh_rate = refresh_rate;
	const st = document.getElementById("saveStatus");
	if(st) st.textContent = "Salvataggio...";
	try{
		const r = await fetch("/settings.json",{
			method:"POST",
			headers:{"Content-Type":"application/json"},
			body:JSON.stringify(settings)
		});
		if(r.ok){
			if(st){
				st.textContent = "Salvato sul dispositivo";
			}
			return;
		}
	}catch(_){
		const blob = new Blob([JSON.stringify(settings,null,2)], {type:"application/json"});
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = "settings.json";
        a.click();
        URL.revokeObjectURL(url);
	}
}

document.addEventListener("DOMContentLoaded",()=>{
	const form = document.getElementById("settingsBox");
	if(form) form.addEventListener("submit", salvaWifi);
	caricaImpostazioni();
	aggiorna();
});

async function aggiorna(){
	while(true){
		await caricaDati();
		await new Promise(r=>setTimeout(r,REFRESH_RATE));
	}
}