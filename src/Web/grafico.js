// grafico.js modificato: genera un grafico per ogni box con il relativo valore
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
            const resp = await fetch('../dati.json', { cache: 'no-store' });
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