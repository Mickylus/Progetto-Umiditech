let REFRESH_RATE = 200; // valore di default in millisecondi
let charts = {};

async function caricaDati(){
	try{
		// Modificare in /dati per leggere i valori direttamente dalla RAM di ESP32
		// Usa dati.json per visualizzare il sito con Live Server
		const r = await fetch('/dati', { cache: "no-store" });
		const d = await r.json();

		const H = document.getElementById("humidity");
		const T = document.getElementById("temperature");
		const V = document.getElementById("volume");

		if(H) H.textContent = d.umidita ?? "";
		if(T) T.textContent = d.temperatura ?? "";
		if(V) V.textContent = d.volume ?? "";

		aggiornaKnob(Number(d.volume || 0));

		const Readstorico = await fetch('storico.json',{cache: 'no-store'});
		const storico = await Readstorico.json();

		const labels = storico.misurazioni.map(x => x.time || ""); // mostra l’orario
		
		charts.humidity.data.labels=labels;
		charts.humidity.data.datasets[0].data = storico.misurazioni.map(x => Number(x.umidita || 0));

		charts.temperature.data.labels = labels;
		charts.temperature.data.datasets[0].data = storico.misurazioni.map(x => Number(x.temperatura || 0));

		charts.humidity.update();
		charts.temperature.update();
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
		document.getElementById("graph_rate").value=w.graph_rate;
		REFRESH_RATE=Number(w.refresh_rate)*1000;
	}catch(_){}
}
async function salvaWifi(e){
	if(e){
		e.preventDefault();
	}
	const ssid =  document.getElementById("ssid").value || "";
	const password = document.getElementById("password").value || "";
	const refresh_rate = Number(document.getElementById("r_rate").value || "1");
	const graph_rate = Number(document.getElementById("graph_rate").value || "1");
	const cb = document.getElementById("setDefaultWifi");
	
	const r = await fetch('settings.json',{cache : 'no-store'});
	settings = await r.json();
	
	settings.ssid = ssid;
	settings.password = password;
	if(cb && cb.checked){
		settings.d_ssid = ssid;
		settings.d_password = password;
	}
	settings.refresh_rate = refresh_rate;
	REFRESH_RATE = refresh_rate*1000;
	settings.graph_rate = graph_rate;
	const st = document.getElementById("saveStatus");
	if(st) st.textContent = "Salvataggio...";
	try{
		const r = await fetch("settings.json",{
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
function escapeHTML(s) {
	return s.replace(/&/g, "&amp;")
			.replace(/</g, "&lt;")
			.replace(/>/g, "&gt;");
}
function formatLog(text){
	text = escapeHTML(text);
	return text
		.replace(/WARNING/g, '<span class="log-warning">WARNING</span>')
		.replace(/ERRORE/g, '<span class="log-error">ERRORE</span>')
		.replace(/\d+/g, '<span class="log-num">$&</span>')
		.replace(/INFO/g, '<span class="log-info">INFO</span>');
}
async function caricaLog(){
	const logFile = await fetch('ESP32.log',{cache : 'no-store'});
	const log = await logFile.text();

	document.getElementById("output_log").innerHTML = formatLog(log);
}
(function(){

	function creaGrafico(idCanvas, label){
		const canvas = document.getElementById(idCanvas);
		if(!canvas){
			return null;
		}
		const ctx = canvas.getContext('2d');

		const data = {
			labels: [label],
			datasets: [{
				label: label,
				data: [0],
				backgroundColor: function(ctx){
					if(ctx.dataset.label === 'Umidità'){
						return 'rgba(70, 110, 117, 0.963)';
					}else{
						return 'rgba(117, 70, 70, 0.96)';
					}
				},
//				backgroundColor: ['rgba(70, 110, 117, 0.963)'],
				borderColor: function(ctx){
					if(ctx.dataset.label === 'Umidità'){
						return 'rgba(70, 110, 117, 0.963)';
					}else{
						return 'rgba(117, 70, 70, 0.96)';
					}
				},
//				borderColor: ['rgba(70, 110, 117, 0.963)'],
				borderWidth: 2,
				tension: 0.25,
				pointRadius: 1,
				hitRadius: 20
			}]
		};

		const unit = (data.datasets[0].label === 'Umidità') ? '%' :(data.datasets[0].label === 'Temperatura') ? '°' : '';

		const config = {
			type: 'line',
			data: data,
			options: {
				responsive: true,
				maintainAspectRatio: false,
				scales: {
					y: { 
						beginAtZero: true,
						ticks: {
							callback: function(value){
								return value + unit;
							}
						}
					},                    
				},
				plugins: {
					legend: { display: false },
					tooltip: {
						callbacks: {
							title: function(ctx) {
									return ctx[0].raw.time;  // mostra l'orario vero
							},
							label: function(ctx) {
								const val = ctx.raw;
								if(ctx.dataset.label === 'Umidità'){
									return val+"%";
								}
								if(ctx.dataset.label === 'Temperatura'){
									return val+"°";
								}
								return val;
							}
						}
					}
				}
			}
		};

		return new Chart(ctx, config);
	}

	async function aggiornaDati(){
		try{
			const resp = await fetch('storico.json', { cache: 'no-store' });
			if(!resp.ok) return;
			const d = await resp.json();

			const first = d.misurazioni[0];

			const valori = {
				humidity: Number(first.umidita || 0),
				temperature: Number(first.temperatura || 0)
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
		charts.humidity = creaGrafico('grafico_humidity', 'Umidità');
		charts.temperature = creaGrafico('grafico_temperature', 'Temperatura');
		aggiornaDati();
		setInterval(aggiornaDati, 1000);
	}

	if(document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
	else start();
})();
function aggiornaKnob(volume){
	const pointer = document.getElementById("knobPointer");
	if(!pointer) return;

	// volume da 0 a 100 → ruota da -135° a +135°
	const deg = -135 + (volume / 100) * 270;
	pointer.style.transform = `rotate(${deg}deg)`;
}
document.addEventListener("DOMContentLoaded",()=>{
	const form = document.getElementById("settingsBox");
	if(form) form.addEventListener("submit", salvaWifi);
	caricaImpostazioni();
	aggiorna();

	const titolo = document.getElementById("log_title");
	const output = document.getElementById("output_log");

	if(titolo && output){
		titolo.style.cursor = "pointer"; // cambio cursore per indicare click
		titolo.addEventListener("click", () => {
			if(output.style.display === "none"){
				output.style.display = "block";
			} else {
				output.style.display = "none";
			}
		});
	}
});
async function aggiorna(){
	while(true){
		await caricaDati();
		await caricaLog();
		await new Promise(r=>setTimeout(r,REFRESH_RATE));
	}
}