let REFRESH_RATE = 200; // valore di default in millisecondi

async function caricaDati(){
	try{
		const r = await fetch("/dati.json");
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
        if(st) st.textContent = "Download creato (settings.json)";
	}
}

(function(){
    let charts = {};

    function creaGrafico(idCanvas, label){
        const canvas = document.getElementById(idCanvas);
        if(!canvas) return null;
        const ctx = canvas.getContext('2d');

        const data = {
            labels: [label],
            datasets: [{
                label: label,
                data: [0],
                backgroundColor: ['#0c72e7ff'],
                borderColor: ['#2163aeff'],
                borderWidth: 1
            }]
        };

        const config = {
            type: 'line',
            data: data,
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: { beginAtZero: true }
                    
                },
                plugins: {
                    legend: { display: false }
                }
            }
        };

        return new Chart(ctx, config);
    }

    async function aggiornaDati(){
        try{
            const resp = await fetch('/dati.json', { cache: 'no-store' });
            if(!resp.ok) return;
            const d = await resp.json();

            const valori = {
                humidity: Number(d.umidita || 0),
                temperature: Number(d.temperatura || 0),
                volume: Number(d.volume || 0)
            };

            for(const key in valori){
                if(charts[key]){
                    charts[key].data.datasets[0].data[0] = valori[key];
                    charts[key].update();
                }
            }
        }catch(e){ console.error('Errore aggiornamento grafici:', e); }
    }

    function start(){
        charts.humidity = creaGrafico('grafico_humidity', 'Umidità - Ultima Ora');
        charts.temperature = creaGrafico('grafico_temperature', 'Temperatura - Ultima Ora');
        charts.volume = creaGrafico('grafico_volume', 'Volume - Ultima Ora');
        aggiornaDati();
        setInterval(aggiornaDati, 1000);
    }

    if(document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
    else start();
})();

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