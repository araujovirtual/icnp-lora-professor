#include "api_professor.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "freertos/semphr.h"
#include "config_wifi.h"

// ============================================================
// CONFIGURACAO DO WIFI / API
// ============================================================

static const char* WIFI_AP_SETUP_SSID = "ICNP_PROFESSOR_SETUP";
static const char* WIFI_AP_SETUP_SENHA = "icnp12345"; // minimo 8 caracteres
static const unsigned long WIFI_TEMPO_CONEXAO_MS = 15000;

// Tempo maximo sem DATA para considerar um Aluno ativo no painel/API.
// Se o no Aluno for desligado ou desconectado, o ultimo estado permanece
// como historico, mas o campo ativo passa para false automaticamente.
static const unsigned long API_ALUNO_ATIVO_TIMEOUT_MS = 8000;

static bool wifiModoSetup = false;
static String wifiSsidAtual = "";

static String wifiScanJsonCache = "{\"redes\":[]}";
static unsigned long wifiScanCacheMs = 0;
static int wifiScanTotalCache = 0;
static bool wifiScanEmAndamento = false;
static bool wifiScanSolicitado = false;
static String wifiScanMensagem = "Lista ainda nao atualizada.";
static SemaphoreHandle_t mutexScanWifi = NULL;
static TaskHandle_t handleTaskScanWifi = NULL;
static const unsigned long WIFI_SCAN_INTERVALO_TAREFA_MS = 250;

static WebServer servidor(80);

static EstadoAlunoAPI estadoAlunos[3];
static SemaphoreHandle_t mutexEstado = NULL;
static bool apiIniciada = false;

// ============================================================
// HTML DO PAINEL
// ============================================================

static const char PAGINA_HTML[] = R"ICNPHTML(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Professor ICNP</title>

<style>
  *{box-sizing:border-box}
  body{margin:0;font-family:Arial,Helvetica,sans-serif;background:#080c12;color:#eaf1fb;overflow-x:hidden}
  .top{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap;padding:10px 14px;background:#0d131c;border-bottom:1px solid #253142}
  .titulo{font-size:22px;font-weight:700}
  .sub{font-size:12px;color:#9daec5;margin-top:3px}
  .btn{background:#172232;border:1px solid #32435a;color:#eaf1fb;border-radius:9px;padding:7px 10px;font-weight:700;cursor:pointer}
  .btn.ativo{background:#143824;border-color:#2d8a59;color:#7bf0a7}
  .btn.rec{background:#351722;border-color:#743044;color:#ff91a5}
  .btn.rec.on{background:#4a0c18;border-color:#ff5c70;color:#ffdce2;box-shadow:0 0 12px rgba(255,92,112,.30)}
  .recdot{display:inline-block;width:9px;height:9px;border-radius:50%;background:#718096;margin-right:5px;vertical-align:middle}
  .recdot.on{background:#ff3857;box-shadow:0 0 9px rgba(255,56,87,.75)}
  .page{padding:10px}
  .status{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px;margin-bottom:10px}
  .box{background:#121a26;border:1px solid #263448;border-radius:13px;padding:9px}
  .k{font-size:12px;color:#9daec5;margin-bottom:4px}
  .v{font-size:17px;font-weight:700}
  .monitores{display:grid;grid-template-columns:1fr;gap:10px}
  .monitores.cols-1{grid-template-columns:1fr}
  .monitores.cols-2{grid-template-columns:repeat(2,minmax(0,1fr))}
  .monitores.cols-3{grid-template-columns:repeat(3,minmax(0,1fr))}
  .monitores.cols-4{grid-template-columns:repeat(4,minmax(0,1fr))}
  .card{background:#101823;border:1px solid #27374c;border-radius:16px;overflow:hidden;min-width:0}
  .head{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;border-bottom:1px solid #27374c}
  .aluno{font-size:22px;font-weight:700}
  .badge{padding:6px 10px;border-radius:999px;font-size:12px;font-weight:700}
  .ok{background:#123a27;color:#79f0aa}
  .ruim{background:#473910;color:#ffd35a}
  .na{background:#4a1b23;color:#ff8a98}
  .body{display:grid;grid-template-columns:2fr 1fr}
  .graficos{padding:10px;background:#0d131c;border-right:1px solid #27374c}
  .graf{background:#151e2b;border:1px solid #29394e;border-radius:13px;padding:8px;margin-bottom:8px}
  .graf:last-child{margin-bottom:0}
  .linha{display:flex;justify-content:space-between;gap:8px;font-size:13px;font-weight:700;margin-bottom:5px}
  .fc{color:#6ff26d}
  .spo2{color:#70d8ff}
  .bat{color:#ffd15c}
  .ppg{color:#d6a3ff}
  canvas{display:block;width:100%;height:150px;background:#0b1119;border-radius:9px}
  .dados{display:grid;grid-template-columns:1fr}
  .grande{padding:13px;border-bottom:1px solid #27374c}
  .rot{font-size:13px;color:#9daec5}
  .num{font-size:52px;font-weight:700;line-height:1}
  .un{font-size:13px;color:#9daec5}
  .mini{display:grid;grid-template-columns:1fr 1fr;gap:7px;padding:8px}
  .mini .box{min-height:58px;padding:7px}
  .mini .v{font-size:15px}
  .rodape{text-align:center;color:#8290a4;font-size:12px;margin-top:8px}

  body.tv{height:100vh;overflow:hidden}
  .tv .top{height:52px;padding:7px 10px}
  .tv .titulo{font-size:19px}
  .tv .sub{font-size:11px}
  .tv .page{height:calc(100vh - 52px);padding:7px;overflow:hidden}
  .tv .status,.tv .rodape{display:none}
  .tv .monitores{height:100%;gap:7px}
  .tv .card{height:100%;display:flex;flex-direction:column}
  .tv .head{height:40px;padding:7px 9px;flex-shrink:0}
  .tv .aluno{font-size:18px}
  .tv .body{display:grid;grid-template-columns:1fr;min-height:0;flex:1}
  .tv .graficos{padding:6px;border-right:none;border-bottom:1px solid #27374c;display:grid;grid-template-rows:1fr 1fr 0.8fr;gap:6px;min-height:0}
  .tv .graf{margin:0;padding:6px;min-height:0;display:flex;flex-direction:column}
  .tv .graf.bateria{display:none}
  .tv .linha{font-size:11px;margin-bottom:3px}
  .tv canvas{height:auto;flex:1;min-height:70px}
  .tv .dados{display:block;flex-shrink:0}
  .tv .grande{display:inline-block;width:50%;vertical-align:top;padding:6px 8px}
  .tv .num{font-size:34px}
  .tv .mini{grid-template-columns:repeat(4,1fr);gap:5px;padding:6px}
  .tv .mini .box{min-height:38px;border-radius:8px}
  .tv .mini .k{font-size:9px}
  .tv .mini .v{font-size:11px}
  .tv .monitores.cols-3 .mini,.tv .monitores.cols-4 .mini{grid-template-columns:repeat(2,1fr)}
  .tv .monitores.cols-3 .num{font-size:28px}
  .tv .monitores.cols-4 .num{font-size:25px}

  @media(max-width:900px){
    .body{grid-template-columns:1fr}
    .graficos{border-right:none;border-bottom:1px solid #27374c}
    .monitores.cols-2,.monitores.cols-3,.monitores.cols-4{grid-template-columns:1fr}
    .tv{overflow:auto;height:auto}
    .tv .page{height:auto;overflow:visible}
  }
</style>
</head>

<body>
<div class="top">
  <div>
    <div class="titulo">Professor ICNP - Monitoramento PPG</div>
    <div class="sub">Tendencias de FC, SpO2, PA estimada e onda PPG do pulso recebidas pelo protocolo ICNP.</div>
  </div>

  <div>
    <span class="sub">Alunos por tela: </span>
    <button class="btn" id="b1" onclick="setCols(1)">1</button>
    <button class="btn" id="b2" onclick="setCols(2)">2</button>
    <button class="btn" id="b3" onclick="setCols(3)">3</button>
    <button class="btn" id="b4" onclick="setCols(4)">4</button>
    <button class="btn" onclick="full()">Tela cheia</button>
    <button class="btn rec" id="recBtn" onclick="recToggle()">Gravar</button>
    <button class="btn" id="recGerarBtn" onclick="recGerarDados()" disabled>Exportar</button>
    <button class="btn" id="recLimparBtn" onclick="recLimpar()" disabled>Limpar</button>
    <span class="sub" id="recStatus"><span class="recdot" id="recDot"></span>REC parado</span>
  </div>
</div>

<div class="page">
  <div id="status"></div>
  <div id="monitores" class="monitores cols-2"></div>
  <div class="rodape">A onda PPG do pulso representa o sinal optico do sensor. FC, SpO2 e PA sao estimativas experimentais; nao representam ECG, diagnostico ou validacao clinica.</div>
</div>

<script>
let estado={alunos:[]};
let cols=Number(localStorage.getItem('icnp_cols')||'2');
let hist={};
let animacaoIniciada=false;
let ultimoFrameDesenho=0;

const JANELA_MS=60000;
const PASSO_TEMPO_MS=5000;
const INTERVALO_API_MS=500;
const INTERVALO_DESENHO_MS=120;
const PPG_RECENTE_MS_EXPORT=4500;
const REC_MAX_AMOSTRAS=7200;

let gravacao={
  ativa:false,
  inicioTs:0,
  fimTs:0,
  inicioIso:'',
  fimIso:'',
  amostras:[],
  ultimaAtualizacaoUi:0
};

function recDuracaoMs(){
  if(gravacao.ativa) return Date.now()-gravacao.inicioTs;
  if(gravacao.inicioTs && gravacao.fimTs) return gravacao.fimTs-gravacao.inicioTs;
  return 0;
}

function recTempoHumano(ms){
  ms=Math.max(0,Number(ms)||0);
  let s=Math.floor(ms/1000);
  let m=Math.floor(s/60);
  s=s%60;
  return String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');
}

function recAtualizarUi(){
  let btn=document.getElementById('recBtn');
  let gerar=document.getElementById('recGerarBtn');
  let limpar=document.getElementById('recLimparBtn');
  let status=document.getElementById('recStatus');
  let dot=document.getElementById('recDot');

  if(btn){
    btn.classList.toggle('on',gravacao.ativa);
    btn.textContent=gravacao.ativa?'Parar e exportar':'Gravar';
  }

  if(gerar) gerar.disabled=gravacao.amostras.length===0;
  if(limpar) limpar.disabled=gravacao.amostras.length===0 && !gravacao.ativa;
  if(dot) dot.classList.toggle('on',gravacao.ativa);

  if(status){
    let dur=recTempoHumano(recDuracaoMs());
    let qtd=gravacao.amostras.length;
    status.innerHTML='<span class="recdot '+(gravacao.ativa?'on':'')+'"></span>'+
      (gravacao.ativa?'REC gravando ':'REC parado ')+dur+' | '+qtd+' amostras';
  }
}

function recToggle(){
  if(gravacao.ativa){
    recParar();
    recGerarDados();
    return;
  }
  recIniciar();
}

function recIniciar(){
  gravacao.ativa=true;
  gravacao.inicioTs=Date.now();
  gravacao.fimTs=0;
  gravacao.inicioIso=new Date(gravacao.inicioTs).toISOString();
  gravacao.fimIso='';
  gravacao.amostras=[];
  recAtualizarUi();
}

function recParar(){
  if(!gravacao.ativa) return;
  gravacao.ativa=false;
  gravacao.fimTs=Date.now();
  gravacao.fimIso=new Date(gravacao.fimTs).toISOString();
  recAtualizarUi();
}

function recLimpar(){
  gravacao.ativa=false;
  gravacao.inicioTs=0;
  gravacao.fimTs=0;
  gravacao.inicioIso='';
  gravacao.fimIso='';
  gravacao.amostras=[];
  recAtualizarUi();
}

function recValor(v){
  return (v===undefined)?null:v;
}

function recCloneAluno(a){
  if(!a) return null;

  return {
    ativo:!!a.ativo,
    aluno:recValor(a.aluno),
    seq:recValor(a.seq),
    ciclo:recValor(a.ciclo),
    fc:recValor(a.fc),
    spo2:recValor(a.spo2),
    ir:recValor(a.ir),
    red:recValor(a.red),
    dedo:recValor(a.dedo),
    qual:recValor(a.qual),
    rssi:recValor(a.rssi),
    snr:recValor(a.snr),
    bat_aluno:recValor(a.bat_aluno),
    energia_professor:recValor(a.energia_professor),
    ack:recValor(a.ack),
    idade_ms:recValor(a.idade_ms),
    tempo_ms:recValor(a.tempo_ms),
    ppg_n:recValor(a.ppg_n),
    ppg_idade_ms:recValor(a.ppg_idade_ms),
    ppg_tempo_ms:recValor(a.ppg_tempo_ms),
    ppg:Array.isArray(a.ppg)?a.ppg.slice():[]
  };
}

function recRegistrar(d){
  if(!gravacao.ativa || !d) return;

  let agora=Date.now();
  let alunos=Array.isArray(d.alunos)?d.alunos.map(recCloneAluno).filter(Boolean):[];

  gravacao.amostras.push({
    idx:gravacao.amostras.length+1,
    t_ms:agora-gravacao.inicioTs,
    iso:new Date(agora).toISOString(),
    sistema:recValor(d.sistema),
    wifi:recValor(d.wifi),
    ip:recValor(d.ip),
    alunos:alunos
  });

  while(gravacao.amostras.length>REC_MAX_AMOSTRAS){
    gravacao.amostras.shift();
  }

  if(agora-gravacao.ultimaAtualizacaoUi>500){
    gravacao.ultimaAtualizacaoUi=agora;
    recAtualizarUi();
  }
}

function recNomeBase(){
  let d=new Date(gravacao.inicioTs || Date.now());
  let pad=n=>String(n).padStart(2,'0');
  return 'icnp_gravacao_'+d.getFullYear()+pad(d.getMonth()+1)+pad(d.getDate())+'_'+pad(d.getHours())+pad(d.getMinutes())+pad(d.getSeconds());
}

function recMontarJson(){
  return {
    tipo:'ICNP_API_GRAVACAO',
    versao:'API_ORIGINAL_EXPORT_BROWSER_V1',
    inicio_iso:gravacao.inicioIso,
    fim_iso:gravacao.fimIso || (gravacao.ativa?new Date().toISOString():''),
    duracao_ms:recDuracaoMs(),
    intervalo_api_ms:INTERVALO_API_MS,
    ppg_recente_ms_referencia:PPG_RECENTE_MS_EXPORT,
    total_amostras:gravacao.amostras.length,
    observacao:'Registro gerado no navegador da API do Professor. FC/SpO2 sao estimativas experimentais; nao usar como validacao clinica.',
    amostras:gravacao.amostras
  };
}

function recCsvEscape(v){
  if(v===null||v===undefined) return 'NA';
  let s=String(v);
  if(s.indexOf(';')>=0 || s.indexOf('"')>=0 || s.indexOf('\n')>=0 || s.indexOf('\r')>=0){
    s='"'+s.replace(/"/g,'""')+'"';
  }
  return s;
}

function recLinhas(){
  let linhas=[];

  gravacao.amostras.forEach(am=>{
    (am.alunos||[]).forEach(a=>{
      let ppg=Array.isArray(a.ppg)?a.ppg:[];
      linhas.push({
        amostra:am.idx,
        t_ms:am.t_ms,
        iso:am.iso,
        sistema:am.sistema,
        wifi:am.wifi,
        ip:am.ip,
        aluno:a.aluno,
        ativo:a.ativo?'SIM':'NAO',
        seq:a.seq,
        ciclo:a.ciclo,
        fc:a.fc,
        spo2:a.spo2,
        sys:a.sys,
        dia:a.dia,
        pa_est:a.pa_est,
        uso:a.uso,
        sinal_ppg:a.sinal_ppg,
        pa_valida:a.pa_valida,
        movimento:a.movimento,
        artefato_ppg:a.artefato_ppg,
        ir:a.ir,
        red:a.red,
        dedo:a.dedo,
        qual:a.qual,
        rssi:a.rssi,
        snr:a.snr,
        bat_aluno:a.bat_aluno,
        energia_professor:a.energia_professor,
        ack:a.ack,
        idade_ms:a.idade_ms,
        tempo_ms:a.tempo_ms,
        ppg_n:a.ppg_n,
        ppg_idade_ms:a.ppg_idade_ms,
        ppg_tempo_ms:a.ppg_tempo_ms,
        ppg_primeiros_16:ppg.slice(0,16).join(','),
        ppg_todos:ppg.join(',')
      });
    });
  });

  return linhas;
}

function recMontarCsv(){
  let campos=['amostra','t_ms','iso','sistema','wifi','ip','aluno','ativo','seq','ciclo','fc','spo2','sys','dia','pa_est','uso','sinal_ppg','pa_valida','movimento','artefato_ppg','ir','red','dedo','qual','rssi','snr','bat_aluno','energia_professor','ack','idade_ms','tempo_ms','ppg_n','ppg_idade_ms','ppg_tempo_ms','ppg_primeiros_16','ppg_todos'];
  let linhas=[campos.join(';')];

  recLinhas().forEach(l=>{
    linhas.push(campos.map(c=>recCsvEscape(l[c])).join(';'));
  });

  return linhas.join('\n');
}

function recMontarTxt(){
  let saida=[];
  saida.push('ICNP_GRAVACAO_TXT');
  saida.push('inicio_iso='+recCsvEscape(gravacao.inicioIso));
  saida.push('fim_iso='+recCsvEscape(gravacao.fimIso || (gravacao.ativa?new Date().toISOString():'')));
  saida.push('duracao_ms='+recDuracaoMs());
  saida.push('total_amostras='+gravacao.amostras.length);
  saida.push('formato=uma linha por aluno por amostra; campos separados por ponto e virgula; sem JSON');
  saida.push('');

  recLinhas().forEach(l=>{
    saida.push(
      'amostra='+recCsvEscape(l.amostra)+';'+
      't_ms='+recCsvEscape(l.t_ms)+';'+
      'iso='+recCsvEscape(l.iso)+';'+
      'aluno='+recCsvEscape(l.aluno)+';'+
      'ativo='+recCsvEscape(l.ativo)+';'+
      'seq='+recCsvEscape(l.seq)+';'+
      'ciclo='+recCsvEscape(l.ciclo)+';'+
      'fc='+recCsvEscape(l.fc)+';'+
      'spo2='+recCsvEscape(l.spo2)+';'+
      'sys='+recCsvEscape(l.sys)+';'+
      'dia='+recCsvEscape(l.dia)+';'+
      'pa_est='+recCsvEscape(l.pa_est)+';'+
      'uso='+recCsvEscape(l.uso)+';'+
      'sinal_ppg='+recCsvEscape(l.sinal_ppg)+';'+
      'pa_valida='+recCsvEscape(l.pa_valida)+';'+
      'movimento='+recCsvEscape(l.movimento)+';'+
      'artefato_ppg='+recCsvEscape(l.artefato_ppg)+';'+
      'ir='+recCsvEscape(l.ir)+';'+
      'red='+recCsvEscape(l.red)+';'+
      'dedo='+recCsvEscape(l.dedo)+';'+
      'qual='+recCsvEscape(l.qual)+';'+
      'rssi='+recCsvEscape(l.rssi)+';'+
      'snr='+recCsvEscape(l.snr)+';'+
      'bat_aluno='+recCsvEscape(l.bat_aluno)+';'+
      'energia_professor='+recCsvEscape(l.energia_professor)+';'+
      'ack='+recCsvEscape(l.ack)+';'+
      'idade_ms='+recCsvEscape(l.idade_ms)+';'+
      'ppg_n='+recCsvEscape(l.ppg_n)+';'+
      'ppg_idade_ms='+recCsvEscape(l.ppg_idade_ms)+';'+
      'ppg_primeiros_16='+recCsvEscape(l.ppg_primeiros_16)
    );
  });

  return saida.join('\n');
}

function recHtmlEscape(v){
  if(v===null||v===undefined) return 'NA';
  return String(v)
    .replace(/&/g,'&amp;')
    .replace(/</g,'&lt;')
    .replace(/>/g,'&gt;')
    .replace(/"/g,'&quot;');
}

function recNumeroValido(v){
  return v!==null && v!==undefined && v!=='' && !isNaN(Number(v));
}

function recGraficoImagem(linhas,aluno,campo,titulo,unidade,minY,maxY){
  let dados=linhas.filter(l=>String(l.aluno)===String(aluno) && recNumeroValido(l[campo]));
  let c=document.createElement('canvas');
  c.width=900;
  c.height=260;
  let x=c.getContext('2d');
  let L=58,T=28,R=22,B=42;
  let W=c.width,H=c.height;
  let PW=W-L-R,PH=H-T-B;

  x.fillStyle='#ffffff';
  x.fillRect(0,0,W,H);
  x.fillStyle='#111827';
  x.font='bold 18px Arial';
  x.textAlign='left';
  x.fillText(titulo+' - Aluno '+aluno,18,20);
  x.strokeStyle='#d1d5db';
  x.lineWidth=1;
  x.strokeRect(L,T,PW,PH);
  x.fillStyle='#374151';
  x.font='11px Arial';
  x.textAlign='right';

  for(let i=0;i<=4;i++){
    let val=maxY-(maxY-minY)*i/4;
    let y=T+PH*i/4;
    x.strokeStyle='#e5e7eb';
    x.beginPath();
    x.moveTo(L,y);
    x.lineTo(L+PW,y);
    x.stroke();
    x.fillStyle='#374151';
    x.fillText(String(Math.round(val)),L-6,y+4);
  }

  if(dados.length<2){
    x.textAlign='center';
    x.fillStyle='#6b7280';
    x.font='16px Arial';
    x.fillText('Sem dados suficientes',W/2,H/2);
    return c.toDataURL('image/png');
  }

  let t0=Number(dados[0].t_ms)||0;
  let t1=Number(dados[dados.length-1].t_ms)||1;
  if(t1<=t0) t1=t0+1;

  x.strokeStyle='#2563eb';
  x.lineWidth=3;
  x.lineJoin='round';
  x.lineCap='round';
  x.beginPath();

  dados.forEach((d,i)=>{
    let val=Math.max(minY,Math.min(maxY,Number(d[campo])));
    let xx=L+((Number(d.t_ms)-t0)/(t1-t0))*PW;
    let yy=T+PH-((val-minY)/(maxY-minY))*PH;
    if(i===0) x.moveTo(xx,yy);
    else x.lineTo(xx,yy);
  });

  x.stroke();

  let ultimo=dados[dados.length-1];
  let uv=Number(ultimo[campo]);
  let ux=L+((Number(ultimo.t_ms)-t0)/(t1-t0))*PW;
  let uy=T+PH-((Math.max(minY,Math.min(maxY,uv))-minY)/(maxY-minY))*PH;
  x.fillStyle='#dc2626';
  x.beginPath();
  x.arc(ux,uy,5,0,Math.PI*2);
  x.fill();

  x.fillStyle='#111827';
  x.font='12px Arial';
  x.textAlign='right';
  x.fillText('Atual: '+uv+' '+unidade,W-20,22);
  x.fillText('Tempo relativo (ms)',L+PW,T+PH+30);

  return c.toDataURL('image/png');
}


function recDadosGrafico(linhas,aluno,campo){
  return linhas.filter(l=>String(l.aluno)===String(aluno) && recNumeroValido(l[campo]));
}

function recAlunoAtivoNaGravacao(linhas,aluno){
  return linhas.some(l=>String(l.aluno)===String(aluno) && String(l.ativo)==='SIM');
}

function recAlunoTemSerieValida(linhas,aluno,campo,minPontos=2){
  return recDadosGrafico(linhas,aluno,campo).length>=minPontos;
}

function recAlunoElegivelGraficos(linhas,aluno){
  return recAlunoAtivoNaGravacao(linhas,aluno) ||
         recAlunoTemSerieValida(linhas,aluno,'fc',2) ||
         recAlunoTemSerieValida(linhas,aluno,'spo2',2) ||
         recAlunoTemSerieValida(linhas,aluno,'bat_aluno',2);
}

function recListaAlunosGraficos(linhas){
  return [...new Set(linhas.map(l=>l.aluno).filter(v=>v!==null&&v!==undefined))]
    .filter(aluno=>recAlunoElegivelGraficos(linhas,aluno))
    .sort((a,b)=>String(a).localeCompare(String(b),undefined,{numeric:true,sensitivity:'base'}));
}

function recDesenharGraficoRetangulo(ctx,dados,left,top,width,height,titulo,unidade,minY,maxY,corLinha){
  let padL=42,padT=24,padR=10,padB=26;
  let px=left+padL, py=top+padT;
  let pw=Math.max(10,width-padL-padR), ph=Math.max(10,height-padT-padB);

  ctx.fillStyle='#ffffff';
  ctx.fillRect(left,top,width,height);
  ctx.strokeStyle='#d1d5db';
  ctx.lineWidth=1;
  ctx.strokeRect(left,top,width,height);

  ctx.fillStyle='#111827';
  ctx.font='bold 14px Arial';
  ctx.textAlign='left';
  ctx.fillText(titulo,left+10,top+16);

  for(let i=0;i<=4;i++){
    let val=maxY-(maxY-minY)*i/4;
    let y=py+ph*i/4;
    ctx.strokeStyle='#e5e7eb';
    ctx.beginPath();
    ctx.moveTo(px,y);
    ctx.lineTo(px+pw,y);
    ctx.stroke();
    ctx.fillStyle='#4b5563';
    ctx.font='10px Arial';
    ctx.textAlign='right';
    ctx.fillText(String(Math.round(val*10)/10),px-4,y+3);
  }

  if(dados.length<2){
    ctx.textAlign='center';
    ctx.fillStyle='#6b7280';
    ctx.font='12px Arial';
    ctx.fillText('Sem dados suficientes',left+width/2,top+height/2);
    return;
  }

  let t0=Number(dados[0].t_ms)||0;
  let t1=Number(dados[dados.length-1].t_ms)||1;
  if(t1<=t0) t1=t0+1;

  ctx.strokeStyle=corLinha||'#2563eb';
  ctx.lineWidth=2;
  ctx.lineJoin='round';
  ctx.lineCap='round';
  ctx.beginPath();

  dados.forEach((d,i)=>{
    let val=Math.max(minY,Math.min(maxY,Number(d.valor)));
    let xx=px+((Number(d.t_ms)-t0)/(t1-t0))*pw;
    let yy=py+ph-((val-minY)/(maxY-minY))*ph;
    if(i===0) ctx.moveTo(xx,yy);
    else ctx.lineTo(xx,yy);
  });
  ctx.stroke();

  let ultimo=dados[dados.length-1];
  let uv=Number(ultimo.valor);
  let ux=px+((Number(ultimo.t_ms)-t0)/(t1-t0))*pw;
  let uy=py+ph-((Math.max(minY,Math.min(maxY,uv))-minY)/(maxY-minY))*ph;
  ctx.fillStyle='#dc2626';
  ctx.beginPath();
  ctx.arc(ux,uy,4,0,Math.PI*2);
  ctx.fill();

  ctx.fillStyle='#111827';
  ctx.font='11px Arial';
  ctx.textAlign='right';
  ctx.fillText('Atual: '+(Math.round(uv*10)/10)+' '+unidade,left+width-8,top+16);
}

function recGraficoImagemTamanho(linhas,aluno,campo,titulo,unidade,minY,maxY,largura,altura){
  let dados=recDadosGrafico(linhas,aluno,campo).map(d=>({t_ms:d.t_ms,valor:Number(d[campo])}));
  let c=document.createElement('canvas');
  c.width=largura||900;
  c.height=altura||260;
  let ctx=c.getContext('2d');
  recDesenharGraficoRetangulo(ctx,dados,0,0,c.width,c.height,titulo+' - Aluno '+aluno,unidade,minY,maxY,'#2563eb');
  return c.toDataURL('image/png');
}

function recMontarPainelGraficosDataUri(){
  let linhas=recLinhas();
  let alunos=recListaAlunosGraficos(linhas);
  if(alunos.length===0) return null;

  let marg=32;
  let chartW=332;
  let chartH=180;
  let gap=14;
  let largura=(marg*2)+(chartW*3)+(gap*2);
  let cabH=72;
  let tituloAlunoH=34;
  let blocoGap=22;
  let blocoH=tituloAlunoH+chartH+blocoGap;
  let altura=cabH+(alunos.length*blocoH)+marg;

  let c=document.createElement('canvas');
  c.width=largura;
  c.height=altura;
  let ctx=c.getContext('2d');

  ctx.fillStyle='#ffffff';
  ctx.fillRect(0,0,largura,altura);

  ctx.fillStyle='#111827';
  ctx.font='bold 22px Arial';
  ctx.textAlign='left';
  ctx.textBaseline='alphabetic';
  ctx.fillText('Gravacao ICNP - Graficos por no ativo',marg,34);
  ctx.font='12px Arial';
  ctx.fillStyle='#374151';
  ctx.fillText('Inicio: '+String(gravacao.inicioIso||''),marg,54);
  ctx.fillText('Duracao: '+String(recDuracaoMs())+' ms | Amostras API: '+String(gravacao.amostras.length),marg+400,54);

  alunos.forEach((aluno,idx)=>{
    let y=cabH+(idx*blocoH);
    let chartsTop=y+tituloAlunoH;
    let blocoTop=y-4;
    let blocoHeight=tituloAlunoH+chartH+8;

    if(idx>0){
      ctx.strokeStyle='#e5e7eb';
      ctx.lineWidth=1;
      ctx.beginPath();
      ctx.moveTo(marg,y-12);
      ctx.lineTo(largura-marg,y-12);
      ctx.stroke();
    }

    ctx.fillStyle='#ffffff';
    ctx.fillRect(marg-8,blocoTop,largura-(marg*2)+16,blocoHeight);

    ctx.fillStyle='#111827';
    ctx.font='bold 18px Arial';
    ctx.textAlign='left';
    ctx.fillText('Aluno '+aluno,marg+6,y+22);

    recDesenharGraficoRetangulo(
      ctx,
      recDadosGrafico(linhas,aluno,'fc').map(d=>({t_ms:d.t_ms,valor:Number(d.fc)})),
      marg,chartsTop,chartW,chartH,'FC','bpm',40,180,'#2563eb'
    );
    recDesenharGraficoRetangulo(
      ctx,
      recDadosGrafico(linhas,aluno,'spo2').map(d=>({t_ms:d.t_ms,valor:Number(d.spo2)})),
      marg+chartW+gap,chartsTop,chartW,chartH,'SpO2','%',80,100,'#059669'
    );
    recDesenharGraficoRetangulo(
      ctx,
      recDadosGrafico(linhas,aluno,'bat_aluno').map(d=>({t_ms:d.t_ms,valor:Number(d.bat_aluno)})),
      marg+((chartW+gap)*2),chartsTop,chartW,chartH,'Bateria','V',3.0,4.3,'#d97706'
    );
  });

  return c.toDataURL('image/png');
}

function recDataUriParaBlob(uri,tipo){
  if(!uri) return null;
  return new Blob([recB64ParaBytes(recDataUriBase64(uri))],{type:tipo||'image/png'});
}

function recMontarPainelGraficosBlob(){
  let uri=recMontarPainelGraficosDataUri();
  return uri?recDataUriParaBlob(uri,'image/png'):null;
}

function recDataUriBase64(uri){
  let s=String(uri||'');
  let p=s.indexOf(',');
  return p>=0?s.substring(p+1):s;
}

function recB64ParaBytes(b64){
  let bin=atob(String(b64||'').replace(/\s/g,''));
  let arr=new Uint8Array(bin.length);
  for(let i=0;i<bin.length;i++) arr[i]=bin.charCodeAt(i);
  return arr;
}

function recXmlEscape(v){
  return String(v===null||v===undefined?'':v)
    .replace(/&/g,'&amp;')
    .replace(/</g,'&lt;')
    .replace(/>/g,'&gt;')
    .replace(/"/g,'&quot;');
}

function recColunaExcel(n){
  let s='';
  n++;
  while(n>0){
    let m=(n-1)%26;
    s=String.fromCharCode(65+m)+s;
    n=Math.floor((n-1)/26);
  }
  return s;
}

function recValorNumericoExcel(v){
  if(v===null||v===undefined||v==='') return null;
  if(String(v).toUpperCase()==='NA') return null;
  let n=Number(v);
  return Number.isFinite(n)?n:null;
}

function recCelulaXlsx(c,r,v){
  let ref=recColunaExcel(c)+r;
  let n=recValorNumericoExcel(v);
  if(n!==null && String(v).trim()!=='' && !String(v).includes('T')){
    return '<c r="'+ref+'"><v>'+String(n)+'</v></c>';
  }
  return '<c r="'+ref+'" t="inlineStr"><is><t>'+recXmlEscape(v)+'</t></is></c>';
}

function recLinhaXlsx(r,vals){
  let xml='<row r="'+r+'">';
  vals.forEach((v,c)=>{xml+=recCelulaXlsx(c,r,v);});
  xml+='</row>';
  return xml;
}

function recCrc32Tabela(){
  let t=[];
  for(let n=0;n<256;n++){
    let c=n;
    for(let k=0;k<8;k++) c=(c&1)?(0xedb88320^(c>>>1)):(c>>>1);
    t[n]=c>>>0;
  }
  return t;
}

const REC_CRC32_TABELA=recCrc32Tabela();

function recCrc32(bytes){
  let c=0xffffffff;
  for(let i=0;i<bytes.length;i++) c=REC_CRC32_TABELA[(c^bytes[i])&0xff]^(c>>>8);
  return (c^0xffffffff)>>>0;
}

function recU16(v){
  let b=new Uint8Array(2);
  let d=new DataView(b.buffer);
  d.setUint16(0,v,true);
  return b;
}

function recU32(v){
  let b=new Uint8Array(4);
  let d=new DataView(b.buffer);
  d.setUint32(0,v>>>0,true);
  return b;
}

function recConcat(partes){
  let total=0;
  partes.forEach(p=>{total+=p.length;});
  let out=new Uint8Array(total);
  let off=0;
  partes.forEach(p=>{out.set(p,off);off+=p.length;});
  return out;
}

function recBytesTexto(s){
  return new TextEncoder().encode(String(s));
}

function recZipCriar(arquivos){
  let locais=[];
  let centrais=[];
  let offset=0;

  arquivos.forEach(arq=>{
    let nome=recBytesTexto(arq.nome);
    let dados=arq.dados instanceof Uint8Array?arq.dados:recBytesTexto(arq.dados);
    let crc=recCrc32(dados);

    let local=recConcat([
      recU32(0x04034b50),recU16(20),recU16(0),recU16(0),recU16(0),recU16(0),
      recU32(crc),recU32(dados.length),recU32(dados.length),recU16(nome.length),recU16(0),nome,dados
    ]);
    locais.push(local);

    let central=recConcat([
      recU32(0x02014b50),recU16(20),recU16(20),recU16(0),recU16(0),recU16(0),recU16(0),
      recU32(crc),recU32(dados.length),recU32(dados.length),recU16(nome.length),recU16(0),recU16(0),
      recU16(0),recU16(0),recU32(0),recU32(offset),nome
    ]);
    centrais.push(central);
    offset+=local.length;
  });

  let centralInicio=offset;
  let centralBytes=recConcat(centrais);
  let fim=recConcat([
    recU32(0x06054b50),recU16(0),recU16(0),recU16(arquivos.length),recU16(arquivos.length),
    recU32(centralBytes.length),recU32(centralInicio),recU16(0)
  ]);
  return recConcat([...locais,centralBytes,fim]);
}

function recMontarXlsxBlob(){
  let linhas=recLinhas();
  let alunos=recListaAlunosGraficos(linhas);
  let campos=['amostra','t_ms','iso','aluno','ativo','seq','ciclo','fc','spo2','sys','dia','pa_est','uso','sinal_ppg','pa_valida','movimento','artefato_ppg','ir','red','dedo','qual','rssi','snr','bat_aluno','energia_professor','ack','idade_ms','ppg_n','ppg_idade_ms','ppg_primeiros_16'];

  let imagens=[];
  alunos.forEach(aluno=>{
    imagens.push({aluno:aluno,tipo:'FC',nome:'grafico_aluno_'+aluno+'_fc.png',b64:recDataUriBase64(recGraficoImagemTamanho(linhas,aluno,'fc','Frequencia cardiaca','bpm',40,180,900,240))});
    imagens.push({aluno:aluno,tipo:'SpO2',nome:'grafico_aluno_'+aluno+'_spo2.png',b64:recDataUriBase64(recGraficoImagemTamanho(linhas,aluno,'spo2','SpO2','%',80,100,900,240))});
    imagens.push({aluno:aluno,tipo:'Bateria',nome:'grafico_aluno_'+aluno+'_bateria.png',b64:recDataUriBase64(recGraficoImagemTamanho(linhas,aluno,'bat_aluno','Bateria do aluno','V',3.0,4.3,900,240))});
  });

  let linhasResumo=[];
  linhasResumo.push(['Gravacao ICNP - API do Professor']);
  linhasResumo.push(['Arquivo Excel XLSX real gerado no navegador. Graficos sao imagens estaticas incorporadas ao arquivo.']);
  linhasResumo.push([]);
  linhasResumo.push(['Inicio ISO',gravacao.inicioIso]);
  linhasResumo.push(['Fim ISO',gravacao.fimIso || (gravacao.ativa?new Date().toISOString():'')]);
  linhasResumo.push(['Duracao ms',recDuracaoMs()]);
  linhasResumo.push(['Total de amostras API',gravacao.amostras.length]);
  linhasResumo.push(['Observacao','FC, SpO2 e PA sao estimativas experimentais; nao representam diagnostico, medicao clinica ou validacao clinica.']);
  linhasResumo.push([]);
  linhasResumo.push(['Graficos incorporados abaixo']);

  let sheet1Rows='';
  linhasResumo.forEach((vals,i)=>{sheet1Rows+=recLinhaXlsx(i+1,vals);});
  let sheet1='<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'+
    '<cols><col min="1" max="1" width="26" customWidth="1"/><col min="2" max="12" width="16" customWidth="1"/></cols>'+
    '<sheetData>'+sheet1Rows+'</sheetData><drawing r:id="rId1"/></worksheet>';

  let dadosRows=recLinhaXlsx(1,campos);
  linhas.forEach((l,i)=>{
    dadosRows+=recLinhaXlsx(i+2,campos.map(c=>l[c]));
  });
  let sheet2='<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'+
    '<cols><col min="1" max="22" width="15" customWidth="1"/></cols><sheetData>'+dadosRows+'</sheetData></worksheet>';

  let anchors='';
  let drawingRels='';
  imagens.forEach((img,i)=>{
    let id=i+1;
    let linhaBase=10+i*15;
    anchors+='<xdr:twoCellAnchor editAs="oneCell">'+
      '<xdr:from><xdr:col>0</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>'+linhaBase+'</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>'+
      '<xdr:to><xdr:col>12</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>'+(linhaBase+13)+'</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:to>'+
      '<xdr:pic><xdr:nvPicPr><xdr:cNvPr id="'+id+'" name="Aluno '+recXmlEscape(img.aluno)+' - '+recXmlEscape(img.tipo)+'"/><xdr:cNvPicPr><a:picLocks noChangeAspect="1"/></xdr:cNvPicPr></xdr:nvPicPr>'+
      '<xdr:blipFill><a:blip r:embed="rId'+id+'"/><a:stretch><a:fillRect/></a:stretch></xdr:blipFill>'+
      '<xdr:spPr><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></xdr:spPr></xdr:pic><xdr:clientData/></xdr:twoCellAnchor>';
    drawingRels+='<Relationship Id="rId'+id+'" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/image'+id+'.png"/>';
  });

  let drawing='<xdr:wsDr xmlns:xdr="http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'+anchors+'</xdr:wsDr>';

  let arquivos=[];
  arquivos.push({nome:'[Content_Types].xml',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/drawings/drawing1.xml" ContentType="application/vnd.openxmlformats-officedocument.drawing+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/></Types>'});
  arquivos.push({nome:'_rels/.rels',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>'});
  arquivos.push({nome:'xl/workbook.xml',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Resumo" sheetId="1" r:id="rId1"/><sheet name="Dados" sheetId="2" r:id="rId2"/></sheets></workbook>'});
  arquivos.push({nome:'xl/_rels/workbook.xml.rels',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/><Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>'});
  arquivos.push({nome:'xl/styles.xml',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="1"><font><sz val="11"/><name val="Arial"/></font></fonts><fills count="1"><fill><patternFill patternType="none"/></fill></fills><borders count="1"><border/></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/></cellXfs></styleSheet>'});
  arquivos.push({nome:'xl/worksheets/sheet1.xml',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'+sheet1});
  arquivos.push({nome:'xl/worksheets/sheet2.xml',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'+sheet2});
  arquivos.push({nome:'xl/worksheets/_rels/sheet1.xml.rels',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing" Target="../drawings/drawing1.xml"/></Relationships>'});
  arquivos.push({nome:'xl/drawings/drawing1.xml',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'+drawing});
  arquivos.push({nome:'xl/drawings/_rels/drawing1.xml.rels',dados:'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'+drawingRels+'</Relationships>'});
  imagens.forEach((img,i)=>{
    arquivos.push({nome:'xl/media/image'+(i+1)+'.png',dados:recB64ParaBytes(img.b64)});
  });

  let zipBytes=recZipCriar(arquivos);
  return new Blob([zipBytes],{type:'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet'});
}

function recBaixar(nome,conteudo,tipo){
  let blob=new Blob([conteudo],{type:tipo});
  recBaixarBlob(nome,blob);
}

function recBaixarBlob(nome,blob){
  let url=URL.createObjectURL(blob);
  let a=document.createElement('a');
  a.href=url;
  a.download=nome;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  setTimeout(()=>URL.revokeObjectURL(url),1500);
}

function recGerarDados(){
  if(gravacao.ativa) recParar();

  if(gravacao.amostras.length===0){
    alert('Nenhuma amostra gravada. Clique em Gravar antes de exportar.');
    recAtualizarUi();
    return;
  }

  let base=recNomeBase();
  let txt=recMontarTxt();
  let xlsx=recMontarXlsxBlob();
  let painelGraficos=recMontarPainelGraficosBlob();

  recBaixar(base+'.txt',txt,'text/plain;charset=utf-8');
  setTimeout(()=>recBaixarBlob(base+'.xlsx',xlsx),300);
  if(painelGraficos){
    setTimeout(()=>recBaixarBlob(base+'_graficos.png',painelGraficos),650);
  }
  recAtualizarUi();
}

function H(id){
  if(!hist[id]){
    hist[id]={
      ultimoRegistroTs:0,
      p:[],
      max:240,
      ppgFonte:[],
      ppgEsteira:[],
      ppgHash:'',
      ppgCursor:0,
      ppgUltimoPasso:0,
      ppgMax:160,
      ppgSemContato:true
    };
  }
  return hist[id];
}

function limparEsteiraPpg(id){
  let h=H(id);
  h.ppgFonte=[];
  h.ppgEsteira=[];
  h.ppgHash='';
  h.ppgCursor=0;
  h.ppgUltimoPasso=0;
  h.ppgSemContato=true;
}

function movimentoRotulo(v){
  if(v==='PULANDO_IMPACTO') return 'PULANDO_IMPACTO';
  if(v==='IMPACTO_VERTICAL') return 'PULANDO_IMPACTO';
  if(v==='ANDANDO') return 'ANDANDO';
  if(v==='MOV_LEVE') return 'MOV_LEVE / MOVIMENTO ALEATORIO';
  if(v==='MOV_APOIADO') return 'MOV_APOIADO / SENTADO';
  return v;
}

function t(v,s=''){
  return (v===null||v===undefined)?'NA':String(v)+s;
}

function hora(){
  let d=new Date();
  return String(d.getHours()).padStart(2,'0')+':'+
         String(d.getMinutes()).padStart(2,'0')+':'+
         String(d.getSeconds()).padStart(2,'0');
}

function horaDeTimestamp(ts){
  let d=new Date(ts);
  return String(d.getHours()).padStart(2,'0')+':'+
         String(d.getMinutes()).padStart(2,'0')+':'+
         String(d.getSeconds()).padStart(2,'0');
}

function okBase(a){
  return a && a.ativo===true && a.dedo===1;
}

function okFisio(a){
  return okBase(a) && Number(a.pa_valida)===1 && a.qual==='OK';
}

function okFc(a){
  return okFisio(a) && a.fc!==null && a.fc!==undefined && Number(a.fc)>0;
}

function okSpo2(a){
  return okFisio(a) && a.spo2!==null && a.spo2!==undefined && Number(a.spo2)>0;
}

function okPa(a){
  return okFisio(a) && a.sys!==null && a.sys!==undefined && Number(a.sys)>0 &&
         a.dia!==null && a.dia!==undefined && Number(a.dia)>0;
}

function okBat(a){
  return a && a.bat_aluno!==null && a.bat_aluno!==undefined && Number(a.bat_aluno)>0;
}

function temPpg(a){
  return a && Array.isArray(a.ppg) && a.ppg.length>=2;
}

function badge(q){
  if(q==='OK') return 'badge ok';
  if(q==='RUIM') return 'badge ruim';
  return 'badge na';
}

// ============================================================
// CORES OPERACIONAIS
// Nao representam validacao clinica; sao apenas faixas visuais.
// ============================================================

function corFCValor(v){
  if(v===null || v===undefined || isNaN(Number(v))) return '#718096';

  v=Number(v);

  if(v>=50 && v<=120) return '#6ff26d';
  if((v>=40 && v<50) || (v>120 && v<=160)) return '#ffd15c';
  return '#ff5c70';
}

function corSpO2Valor(v){
  if(v===null || v===undefined || isNaN(Number(v))) return '#718096';

  v=Number(v);

  if(v>=95) return '#6ff26d';
  if(v>=90) return '#ffd15c';
  return '#ff5c70';
}

function corBatValor(v){
  if(v===null || v===undefined || isNaN(Number(v))) return '#718096';

  v=Number(v);

  if(v>=3.50) return '#6ff26d';
  if(v>=3.20) return '#ffd15c';
  return '#ff5c70';
}

function corPorCampo(campo,v,corPadrao){
  if(campo==='fc') return corFCValor(v);
  if(campo==='spo2') return corSpO2Valor(v);
  if(campo==='bat') return corBatValor(v);
  return corPadrao;
}

function addHist(a){
  if(!a) return;

  let h=H(a.aluno);
  let agora=Date.now();

  if(agora-h.ultimoRegistroTs<900) return;
  h.ultimoRegistroTs=agora;

  h.p.push({
    tempo:hora(),
    ts:agora,
    ciclo:a.ciclo,
    fc:okFc(a)?Number(a.fc):null,
    spo2:okSpo2(a)?Number(a.spo2):null,
    bat:okBat(a)?Number(a.bat_aluno):null
  });

  let limite=agora-(JANELA_MS*3);

  while(h.p.length>0 && h.p[0].ts<limite){
    h.p.shift();
  }

  while(h.p.length>h.max){
    h.p.shift();
  }
}

function atualizarFilaPpg(a){
  if(!a) return;

  let h=H(a.aluno);
  let ppgValido = okBase(a) && Array.isArray(a.ppg) && a.ppg.length>=2;
  let ppgRecente = a.ppg_idade_ms===null || a.ppg_idade_ms===undefined || Number(a.ppg_idade_ms)<=4500;

  if(!ppgValido || !ppgRecente){
    limparEsteiraPpg(a.aluno);
    return;
  }

  let hash=a.ppg.join(',');

  if(hash!==h.ppgHash){
    h.ppgHash=hash;
    h.ppgFonte=a.ppg.slice();
    h.ppgCursor=0;
    h.ppgSemContato=false;
  }
}

function setCols(n){
  cols=n;
  localStorage.setItem('icnp_cols',String(n));

  let m=document.getElementById('monitores');
  if(m) m.className='monitores cols-'+n;

  document.body.classList.toggle('tv',n>=2);

  for(let i=1;i<=4;i++){
    let b=document.getElementById('b'+i);
    if(b) b.classList.toggle('ativo',i===n);
  }

  setTimeout(drawAll,80);
}

function full(){
  if(!document.fullscreenElement) document.documentElement.requestFullscreen();
  else document.exitFullscreen();
}

function renderStatus(d){
  document.getElementById('status').innerHTML =
    '<div class="status">'+
      '<div class="box"><div class="k">Sistema</div><div class="v">'+t(d.sistema)+'</div></div>'+
      '<div class="box"><div class="k">Wi-Fi</div><div class="v">'+t(d.wifi)+'</div></div>'+
      '<div class="box"><div class="k">IP</div><div class="v">'+t(d.ip)+'</div></div>'+
      '<div class="box"><div class="k">Endpoint</div><div class="v">/api/status</div></div>'+
    '</div>';
}

function card(a){
  let id=a.aluno;
  H(id);

  let fc=okFc(a)?a.fc:'NA';
  let sp=okSpo2(a)?a.spo2:'NA';
  let pa=okPa(a)?(a.sys+'x'+a.dia):'NA';

  let corFcGrande=okFc(a)?corFCValor(a.fc):'#718096';
  let corSpGrande=okSpo2(a)?corSpO2Valor(a.spo2):'#718096';
  let corBatGrande=okBat(a)?corBatValor(a.bat_aluno):'#718096';
  let ppgTxt=(okBase(a) && temPpg(a))?(a.ppg.length+' pts'):'sem contato';
  let ppgIdade=(a.ppg_idade_ms!==null && a.ppg_idade_ms!==undefined)?a.ppg_idade_ms+' ms':'NA';

  return ''+
  '<div class="card">'+
    '<div class="head">'+
      '<div class="aluno">Aluno '+id+'</div>'+
      '<div class="'+badge(a.qual)+'">'+t(a.qual)+'</div>'+
    '</div>'+

    '<div class="body">'+
      '<div class="graficos">'+

        '<div class="graf">'+
          '<div class="linha">'+
            '<span class="fc">Tendencia da FC</span>'+
            '<span style="color:'+corFcGrande+'">'+(okFc(a)?a.fc+' bpm':'sem FC valida')+'</span>'+
          '</div>'+
          '<canvas id="fc'+id+'"></canvas>'+
        '</div>'+

        '<div class="graf">'+
          '<div class="linha">'+
            '<span class="spo2">Tendencia da SpO2</span>'+
            '<span style="color:'+corSpGrande+'">'+(okSpo2(a)?a.spo2+' %':'sem SpO2 valida')+'</span>'+
          '</div>'+
          '<canvas id="sp'+id+'"></canvas>'+
        '</div>'+

        '<div class="graf ppg">'+
          '<div class="linha">'+
            '<span class="ppg">Onda PPG do pulso</span>'+
            '<span>'+ppgTxt+'</span>'+
          '</div>'+
          '<canvas id="ppg'+id+'"></canvas>'+
        '</div>'+

        '<div class="graf bateria">'+
          '<div class="linha">'+
            '<span class="bat">Bateria: tempo x V</span>'+
            '<span style="color:'+corBatGrande+'">'+t(a.bat_aluno,' V')+'</span>'+
          '</div>'+
          '<canvas id="bt'+id+'"></canvas>'+
        '</div>'+

      '</div>'+

      '<div class="dados">'+
        '<div class="grande">'+
          '<div class="rot">FC</div>'+
          '<div class="num" style="color:'+corFcGrande+'">'+fc+'</div>'+
          '<div class="un">bpm</div>'+
        '</div>'+

        '<div class="grande">'+
          '<div class="rot">SpO2</div>'+
          '<div class="num" style="color:'+corSpGrande+'">'+sp+'</div>'+
          '<div class="un">%</div>'+
        '</div>'+

        '<div class="mini">'+
          '<div class="box"><div class="k">Ativo</div><div class="v">'+(a.ativo?'SIM':'NAO')+'</div></div>'+
          '<div class="box"><div class="k">Dedo</div><div class="v">'+t(a.dedo)+'</div></div>'+
          '<div class="box"><div class="k">Qual</div><div class="v">'+t(a.qual)+'</div></div>'+
          '<div class="box"><div class="k">PA Est.</div><div class="v">'+pa+' mmHg</div></div>'+
          '<div class="box"><div class="k">Uso</div><div class="v">'+t(a.uso)+'</div></div>'+
          '<div class="box"><div class="k">Sinal PPG</div><div class="v">'+t(a.sinal_ppg)+'</div></div>'+
          '<div class="box"><div class="k">PA valida</div><div class="v">'+(Number(a.pa_valida)===1?'SIM':'NAO')+'</div></div>'+
          '<div class="box"><div class="k">Movimento</div><div class="v">'+t(movimentoRotulo(a.movimento))+'</div></div>'+
          '<div class="box"><div class="k">Artefato</div><div class="v">'+t(a.artefato_ppg)+'</div></div>'+
          '<div class="box"><div class="k">Ciclo/Seq</div><div class="v">'+t(a.ciclo)+'/'+t(a.seq)+'</div></div>'+
          '<div class="box"><div class="k">Bat Aluno</div><div class="v" style="color:'+corBatGrande+'">'+t(a.bat_aluno,' V')+'</div></div>'+
          '<div class="box"><div class="k">Energia Prof</div><div class="v">'+t(a.energia_professor,' V')+'</div></div>'+
          '<div class="box"><div class="k">RSSI/SNR</div><div class="v">'+t(a.rssi)+'/'+t(a.snr)+'</div></div>'+
          '<div class="box"><div class="k">PPG idade</div><div class="v">'+ppgIdade+'</div></div>'+
        '</div>'+

      '</div>'+
    '</div>'+
  '</div>';
}

function prep(id){
  let c=document.getElementById(id);
  if(!c) return null;

  let r=c.getBoundingClientRect();
  let d=window.devicePixelRatio||1;

  c.width=r.width*d;
  c.height=r.height*d;

  let x=c.getContext('2d');
  x.setTransform(d,0,0,d,0,0);

  return {c:c,x:x,w:r.width,h:r.height};
}

function grade(x,w,h,L,T,R,B){
  let PW=w-L-R;
  let PH=h-T-B;

  x.strokeStyle='#263448';
  x.lineWidth=1;

  for(let i=0;i<=4;i++){
    let y=T+PH*i/4;
    x.beginPath();
    x.moveTo(L,y);
    x.lineTo(L+PW,y);
    x.stroke();
  }

  for(let i=0;i<=4;i++){
    let xx=L+PW*i/4;
    x.beginPath();
    x.moveTo(xx,T);
    x.lineTo(xx,T+PH);
    x.stroke();
  }

  x.strokeStyle='#74849a';
  x.beginPath();
  x.moveTo(L,T);
  x.lineTo(L,T+PH);
  x.lineTo(L+PW,T+PH);
  x.stroke();
}

function sem(id,msg){
  let o=prep(id);
  if(!o) return;

  let x=o.x,w=o.w,h=o.h;

  x.clearRect(0,0,w,h);
  grade(x,w,h,44,12,12,38);

  x.fillStyle='#718096';
  x.font='bold 13px Arial';
  x.textAlign='center';
  x.textBaseline='middle';
  x.fillText(msg,w/2,h/2);
}

function semPpg(id,msg){
  let o=prep(id);
  if(!o) return;

  let x=o.x,w=o.w,h=o.h;

  x.clearRect(0,0,w,h);
  grade(x,w,h,20,10,10,20);

  x.fillStyle='#718096';
  x.font='bold 13px Arial';
  x.textAlign='center';
  x.textBaseline='middle';
  x.fillText(msg,w/2,h/2);
}

function ultVisivel(p,campo,t0,t1){
  for(let i=p.length-1;i>=0;i--){
    if(!p[i].ts) continue;
    if(p[i].ts<t0 || p[i].ts>t1) continue;

    let v=p[i][campo];
    if(v!==null && v!==undefined){
      return v;
    }
  }
  return null;
}

function textoRetanguloEstimado(txt, cx, baselineY){
  let largura=(String(txt).length*7)+8;
  let altura=13;

  return {
    x:cx-(largura/2),
    y:baselineY-altura,
    w:largura,
    h:altura
  };
}

function colide(a,b){
  return !(
    a.x+a.w<b.x ||
    b.x+b.w<a.x ||
    a.y+a.h<b.y ||
    b.y+b.h<a.y
  );
}

function dentroPlot(rect,L,T,PW,PH){
  return (
    rect.x>=L+3 &&
    rect.x+rect.w<=L+PW-3 &&
    rect.y>=T+3 &&
    rect.y+rect.h<=T+PH-22
  );
}

function podeDesenhar(rect,ocupados,L,T,PW,PH){
  if(!dentroPlot(rect,L,T,PW,PH)) return false;

  for(let i=0;i<ocupados.length;i++){
    if(colide(rect,ocupados[i])) return false;
  }

  return true;
}

function graf(id,p,campo,corPadrao,minY,maxY,un,msg){
  let o=prep(id);
  if(!o) return;

  let x=o.x,w=o.w,h=o.h;
  let L=44,T=12,R=12,B=34;
  let PW=w-L-R;
  let PH=h-T-B;

  let agora=Date.now();
  let t1=agora;
  let t0=agora-JANELA_MS;

  let valid=p.filter(q=>{
    return q && q.ts && q.ts>=t0 && q.ts<=t1 && q[campo]!==null && q[campo]!==undefined;
  });

  if(valid.length<2){
    sem(id,msg);
    return;
  }

  x.clearRect(0,0,w,h);

  // Faixas visuais discretas. Sao referencias operacionais, nao clinicas.
  if(campo==='fc'){
    pintarFaixaY(x,L,T,PW,PH,minY,maxY,40,50,'rgba(255,211,90,0.10)');
    pintarFaixaY(x,L,T,PW,PH,minY,maxY,50,120,'rgba(111,242,109,0.07)');
    pintarFaixaY(x,L,T,PW,PH,minY,maxY,120,160,'rgba(255,211,90,0.10)');
    pintarFaixaY(x,L,T,PW,PH,minY,maxY,160,180,'rgba(255,92,112,0.09)');
  }

  if(campo==='spo2'){
    pintarFaixaY(x,L,T,PW,PH,minY,maxY,95,100,'rgba(111,242,109,0.08)');
    pintarFaixaY(x,L,T,PW,PH,minY,maxY,90,95,'rgba(255,211,90,0.09)');
  }

  grade(x,w,h,L,T,R,B);

  x.fillStyle='#9daec5';
  x.font='10px Arial';
  x.textAlign='right';
  x.textBaseline='middle';

  for(let i=0;i<=4;i++){
    let val=maxY-(maxY-minY)*i/4;
    let y=T+PH*i/4;
    x.fillText(String(Math.round(val)),L-6,y);
  }

  x.save();
  x.translate(10,T+PH/2);
  x.rotate(-Math.PI/2);
  x.textAlign='center';
  x.fillText(un,0,0);
  x.restore();

  let primeiroTick=Math.floor(t0/PASSO_TEMPO_MS)*PASSO_TEMPO_MS;
  let ultimoTick=t1+PASSO_TEMPO_MS;

  x.save();
  x.beginPath();
  x.rect(L, T+PH+3, PW, B-2);
  x.clip();

  x.fillStyle='#9daec5';
  x.font='10px Arial';
  x.textAlign='center';
  x.textBaseline='top';

  for(let tick=primeiroTick;tick<=ultimoTick;tick+=PASSO_TEMPO_MS){
    let xx=L+((tick-t0)/(t1-t0))*PW;

    if(xx>=L-45 && xx<=L+PW+45){
      x.strokeStyle='rgba(255,255,255,0.10)';
      x.lineWidth=1;
      x.beginPath();
      x.moveTo(xx,T);
      x.lineTo(xx,T+PH);
      x.stroke();

      x.fillText(horaDeTimestamp(tick),xx,T+PH+9);
    }
  }

  x.restore();

  x.strokeStyle='rgba(255,255,255,0.30)';
  x.lineWidth=1;
  x.beginPath();
  x.moveTo(L+PW,T);
  x.lineTo(L+PW,T+PH);
  x.stroke();

  x.save();
  x.beginPath();
  x.rect(L,T,PW,PH);
  x.clip();

  x.lineWidth=3;
  x.lineCap='round';
  x.lineJoin='round';

  let anterior=null;
  let ultimoPonto=null;

  for(let i=0;i<p.length;i++){
    let ponto=p[i];
    if(!ponto || !ponto.ts) continue;

    let vOriginal=ponto[campo];

    if(vOriginal===null || vOriginal===undefined){
      anterior=null;
      continue;
    }

    if(ponto.ts<t0 || ponto.ts>t1){
      anterior=null;
      continue;
    }

    let v=Math.max(minY,Math.min(maxY,Number(vOriginal)));
    let xx=L+((ponto.ts-t0)/(t1-t0))*PW;
    let yy=T+PH-((v-minY)/(maxY-minY))*PH;
    let corAtual=corPorCampo(campo,vOriginal,corPadrao);

    if(anterior!==null){
      x.strokeStyle=corAtual;
      x.shadowColor=corAtual;
      x.shadowBlur=6;
      x.beginPath();
      x.moveTo(anterior.x,anterior.y);
      x.lineTo(xx,yy);
      x.stroke();
      x.shadowBlur=0;
    }

    x.fillStyle=corAtual;
    x.beginPath();
    x.arc(xx,yy,3.5,0,Math.PI*2);
    x.fill();

    anterior={x:xx,y:yy};
    ultimoPonto={x:xx,y:yy,v:vOriginal,cor:corAtual};
  }

  // Marcador vivo do ultimo valor, estilo monitor, sem inventar onda.
  if(ultimoPonto){
    let pulso=1.0+0.35*Math.sin(Date.now()/180);
    x.strokeStyle=ultimoPonto.cor;
    x.fillStyle=ultimoPonto.cor;
    x.lineWidth=2;
    x.beginPath();
    x.arc(ultimoPonto.x,ultimoPonto.y,6*pulso,0,Math.PI*2);
    x.stroke();
    x.beginPath();
    x.arc(ultimoPonto.x,ultimoPonto.y,3.5,0,Math.PI*2);
    x.fill();
  }

  x.restore();

  // Marcadores discretos a cada 5 segundos para FC e SpO2.
  // Mostram valores reais recebidos mais proximos de cada tick, sem criar onda artificial.
  let ocupados=[];
  ocupados.push({x:w-155,y:0,w:155,h:25});

  for(let tick=primeiroTick;tick<=ultimoTick;tick+=PASSO_TEMPO_MS){
    let tickX=L+((tick-t0)/(t1-t0))*PW;

    if(tickX<L+34 || tickX>L+PW-34) continue;

    let melhor=null;
    let menorDif=999999;

    for(let i=0;i<p.length;i++){
      let ponto=p[i];

      if(!ponto || !ponto.ts) continue;
      if(ponto.ts<t0 || ponto.ts>t1) continue;

      let v=ponto[campo];
      if(v===null || v===undefined) continue;

      let dif=Math.abs(ponto.ts-tick);

      if(dif<menorDif){
        menorDif=dif;
        melhor=ponto;
      }
    }

    if(!melhor || menorDif>2500) continue;

    let valorOriginal=Number(melhor[campo]);
    let vv=Math.max(minY,Math.min(maxY,valorOriginal));
    let pontoX=L+((melhor.ts-t0)/(t1-t0))*PW;
    let pontoY=T+PH-((vv-minY)/(maxY-minY))*PH;
    let textoValor=String(valorOriginal);
    let corMarcador=corPorCampo(campo,valorOriginal,corPadrao);

    x.save();
    x.beginPath();
    x.rect(L,T,PW,PH);
    x.clip();

    x.strokeStyle='rgba(255,255,255,0.16)';
    x.lineWidth=1;
    x.setLineDash([3,4]);
    x.beginPath();
    x.moveTo(tickX,T);
    x.lineTo(tickX,T+PH);
    x.stroke();
    x.setLineDash([]);

    x.fillStyle=corMarcador;
    x.beginPath();
    x.arc(pontoX,pontoY,4.2,0,Math.PI*2);
    x.fill();

    x.restore();

    let candidatos=[
      {x:pontoX,    y:pontoY-10},
      {x:pontoX,    y:pontoY-24},
      {x:pontoX-28, y:pontoY-10},
      {x:pontoX+28, y:pontoY-10},
      {x:pontoX,    y:pontoY+22},
      {x:pontoX-32, y:pontoY+22},
      {x:pontoX+32, y:pontoY+22}
    ];

    let escolhido=null;
    let escolhidoRect=null;

    for(let c=0;c<candidatos.length;c++){
      let cand=candidatos[c];

      if(cand.y>T+PH-16) cand.y=T+PH-18;
      if(cand.y<T+16) cand.y=T+18;

      let rect=textoRetanguloEstimado(textoValor,cand.x,cand.y);

      if(podeDesenhar(rect,ocupados,L,T,PW,PH)){
        escolhido=cand;
        escolhidoRect=rect;
        break;
      }
    }

    if(escolhido && escolhidoRect){
      x.fillStyle='rgba(8,12,18,0.82)';
      x.fillRect(escolhidoRect.x-3,escolhidoRect.y-2,escolhidoRect.w+6,escolhidoRect.h+4);

      x.fillStyle=corMarcador;
      x.font='bold 10px Arial';
      x.textAlign='center';
      x.textBaseline='bottom';
      x.fillText(textoValor,escolhido.x,escolhido.y);

      ocupados.push({
        x:escolhidoRect.x-5,
        y:escolhidoRect.y-4,
        w:escolhidoRect.w+10,
        h:escolhidoRect.h+8
      });
    }
  }

  let u=ultVisivel(p,campo,t0,t1);

  if(u!==null){
    x.fillStyle=corPorCampo(campo,u,corPadrao);
    x.font='bold 12px Arial';
    x.textAlign='right';
    x.textBaseline='top';
    x.fillText('Atual: '+u+' '+un,w-8,5);
  }
}

function pintarFaixaY(x,L,T,PW,PH,minY,maxY,inicio,fim,cor){
  let a=Math.max(minY,Math.min(maxY,inicio));
  let b=Math.max(minY,Math.min(maxY,fim));
  if(b<=minY || a>=maxY || b<=a) return;

  let y1=T+PH-((b-minY)/(maxY-minY))*PH;
  let y2=T+PH-((a-minY)/(maxY-minY))*PH;

  x.fillStyle=cor;
  x.fillRect(L,y1,PW,y2-y1);
}

function desenharPpg(id,dados,idadeMs){
  let o=prep(id);
  if(!o) return;

  let x=o.x,w=o.w,h=o.h;
  let L=20,T=10,R=10,B=20;
  let PW=w-L-R;
  let PH=h-T-B;

  let alunoId=String(id).replace('ppg','');
  let hst=H(alunoId);

  let ppgRecente = idadeMs===null || idadeMs===undefined || Number(idadeMs)<=4500;

  if(hst.ppgSemContato || !ppgRecente || !hst.ppgFonte || hst.ppgFonte.length<2){
    semPpg(id,'sem contato optico');
    return;
  }

  let agora=Date.now();

  if(hst.ppgUltimoPasso===0){
    hst.ppgUltimoPasso=agora;
  }

  // Playback local da janela PPG: efeito esteira sem aumentar trafego LoRa.
  let intervaloAmostraMs=55;
  let passos=Math.floor((agora-hst.ppgUltimoPasso)/intervaloAmostraMs);

  if(passos>0){
    hst.ppgUltimoPasso+=passos*intervaloAmostraMs;

    for(let k=0;k<passos;k++){
      let v=Number(hst.ppgFonte[hst.ppgCursor]);

      if(isNaN(v)) v=0;
      if(v<0) v=0;
      if(v>255) v=255;

      hst.ppgEsteira.push(v);

      if(hst.ppgEsteira.length>hst.ppgMax){
        hst.ppgEsteira.shift();
      }

      hst.ppgCursor++;

      if(hst.ppgCursor>=hst.ppgFonte.length){
        hst.ppgCursor=0;
      }
    }
  }

  if(hst.ppgEsteira.length<2){
    semPpg(id,'aguardando onda PPG');
    return;
  }

  x.clearRect(0,0,w,h);
  grade(x,w,h,L,T,R,B);

  x.fillStyle='#9daec5';
  x.font='10px Arial';
  x.textAlign='right';
  x.textBaseline='middle';
  x.fillText('alto',L-4,T+5);
  x.fillText('baixo',L-4,T+PH-5);

  x.save();
  x.beginPath();
  x.rect(L,T,PW,PH);
  x.clip();

  x.strokeStyle='#d6a3ff';
  x.shadowColor='#d6a3ff';
  x.shadowBlur=7;
  x.lineWidth=2.5;
  x.lineCap='round';
  x.lineJoin='round';
  x.beginPath();

  let dadosTela=hst.ppgEsteira;

  for(let i=0;i<dadosTela.length;i++){
    let v=Number(dadosTela[i]);

    if(isNaN(v)) v=0;
    if(v<0) v=0;
    if(v>255) v=255;

    let xx=L+(i/(hst.ppgMax-1))*PW;
    let yy=T+PH-(v/255)*PH;

    if(i===0) x.moveTo(xx,yy);
    else x.lineTo(xx,yy);
  }

  x.stroke();
  x.shadowBlur=0;

  let ultimo=dadosTela[dadosTela.length-1];
  let yUlt=T+PH-(ultimo/255)*PH;
  let pulso=1.0+0.35*Math.sin(Date.now()/150);

  x.fillStyle='#d6a3ff';
  x.strokeStyle='#d6a3ff';
  x.lineWidth=2;
  x.beginPath();
  x.arc(L+PW,yUlt,6*pulso,0,Math.PI*2);
  x.stroke();
  x.beginPath();
  x.arc(L+PW,yUlt,3,0,Math.PI*2);
  x.fill();

  x.restore();

  x.fillStyle='#9daec5';
  x.font='10px Arial';
  x.textAlign='right';
  x.textBaseline='top';

  let txt='onda PPG ativa';
  if(idadeMs!==null && idadeMs!==undefined){
    txt+=' | '+idadeMs+' ms';
  }

  x.fillText(txt,w-8,5);
}

function drawAll(){
  if(!estado.alunos) return;

  estado.alunos.forEach(a=>{
    let p=H(a.aluno).p;

    graf('fc'+a.aluno,p,'fc','#6ff26d',40,180,'bpm','sem tendencia de FC valida');
    graf('sp'+a.aluno,p,'spo2','#70d8ff',90,100,'%','sem tendencia de SpO2 valida');
    graf('bt'+a.aluno,p,'bat','#ffd15c',3.0,4.2,'V','sem historico de bateria');
    desenharPpg('ppg'+a.aluno,a.ppg,a.ppg_idade_ms);
  });
}

function animarGraficos(ts){
  if(ts-ultimoFrameDesenho>=INTERVALO_DESENHO_MS){
    drawAll();
    ultimoFrameDesenho=ts;
  }

  requestAnimationFrame(animarGraficos);
}

async function atualizar(){
  try{
    let r=await fetch('/api/status');
    let d=await r.json();

    if(!Array.isArray(d.alunos)){
      d.alunos=[];
    }

    estado=d;
    recRegistrar(d);

    renderStatus(d);

    d.alunos.forEach(a=>{
      addHist(a);
      atualizarFilaPpg(a);
    });

    document.getElementById('monitores').innerHTML=d.alunos.map(card).join('');

    setCols(cols);
    drawAll();

    if(!animacaoIniciada){
      animacaoIniciada=true;
      requestAnimationFrame(animarGraficos);
    }

  } catch(e){
    document.getElementById('monitores').innerHTML =
      '<div class="card"><div class="head"><div class="aluno">Falha ao ler /api/status</div></div></div>';
  }
}

window.addEventListener('resize',()=>setTimeout(drawAll,100));

setInterval(atualizar,INTERVALO_API_MS);

setCols(cols);
recAtualizarUi();
atualizar();
</script>
</body>
</html>
)ICNPHTML";

// ============================================================
// FUNCOES AUXILIARES DE JSON
// ============================================================

static String aspas(const String& valor) {
  String s;
  s += char(34);
  for (size_t i = 0; i < valor.length(); i++) {
    char c = valor.charAt(i);
    if (c == '\\' || c == '"') {
      s += '\\';
      s += c;
    } else if (c == '\n') {
      s += "\\n";
    } else if (c == '\r') {
      s += "\\r";
    } else if (c == '\t') {
      s += "\\t";
    } else {
      s += c;
    }
  }
  s += char(34);
  return s;
}

static void separador(String& json, bool& primeiro) {
  if (!primeiro) {
    json += ',';
  }
  primeiro = false;
}

static void campoBool(String& json, bool& primeiro, const char* nome, bool valor) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';
  json += valor ? "true" : "false";
}

static void campoInt(String& json, bool& primeiro, const char* nome, int valor) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';
  json += String(valor);
}

static void campoULong(String& json, bool& primeiro, const char* nome, unsigned long valor) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';
  json += String(valor);
}

static void campoIntNA(String& json, bool& primeiro, const char* nome, int valor) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';

  if (valor >= 0) json += String(valor);
  else json += "null";
}

static void campoLongNA(String& json, bool& primeiro, const char* nome, long valor) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';

  if (valor >= 0) json += String(valor);
  else json += "null";
}

static void campoFloat(String& json, bool& primeiro, const char* nome, float valor, int casas) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';
  json += String(valor, casas);
}

static void campoFloatNA(String& json, bool& primeiro, const char* nome, float valor, int casas) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';

  if (valor >= 0.0f) json += String(valor, casas);
  else json += "null";
}

static void campoTexto(String& json, bool& primeiro, const char* nome, const String& valor) {
  separador(json, primeiro);
  json += aspas(nome);
  json += ':';
  json += aspas(valor);
}

static void campoPpgArray(String& json, bool& primeiro, const String& ppg) {
  separador(json, primeiro);
  json += aspas("ppg");
  json += ':';

  if (ppg.length() > 0) {
    json += '[';
    json += ppg;
    json += ']';
  } else {
    json += "[]";
  }
}

// ============================================================
// CACHE / MUTEX DA VARREDURA WI-FI
// ============================================================

static void garantirMutexScanWifi() {
  if (mutexScanWifi == NULL) {
    mutexScanWifi = xSemaphoreCreateMutex();
  }
}

static void publicarEstadoScan(bool emAndamento, const String& mensagem) {
  garantirMutexScanWifi();

  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    wifiScanEmAndamento = emAndamento;
    wifiScanMensagem = mensagem;
    xSemaphoreGive(mutexScanWifi);
  }
}

static void publicarCacheScan(const String& json, int total, const String& mensagem) {
  garantirMutexScanWifi();

  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    wifiScanJsonCache = json;
    wifiScanTotalCache = total;
    wifiScanCacheMs = millis();
    wifiScanMensagem = mensagem;
    wifiScanEmAndamento = false;
    xSemaphoreGive(mutexScanWifi);
  }
}

static String obterCacheScan() {
  garantirMutexScanWifi();

  String cache = "{\"redes\":[]}";
  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    cache = wifiScanJsonCache;
    xSemaphoreGive(mutexScanWifi);
  }

  return cache;
}

static int obterTotalCacheScan() {
  garantirMutexScanWifi();

  int total = 0;
  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    total = wifiScanTotalCache;
    xSemaphoreGive(mutexScanWifi);
  }

  return total;
}

static bool obterScanEmAndamento() {
  garantirMutexScanWifi();

  bool valor = false;
  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    valor = wifiScanEmAndamento;
    xSemaphoreGive(mutexScanWifi);
  }

  return valor;
}

static String obterMensagemScan() {
  garantirMutexScanWifi();

  String msg;
  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    msg = wifiScanMensagem;
    xSemaphoreGive(mutexScanWifi);
  }

  return msg;
}

static unsigned long obterIdadeCacheScanMs() {
  garantirMutexScanWifi();

  unsigned long idade = 0;
  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    idade = wifiScanCacheMs > 0 ? (millis() - wifiScanCacheMs) : 0;
    xSemaphoreGive(mutexScanWifi);
  }

  return idade;
}

// ============================================================
// ESTADO / MUTEX
// ============================================================

static void garantirMutexEstado() {
  if (mutexEstado == NULL) {
    mutexEstado = xSemaphoreCreateMutex();
  }
}

static void limparEstadoAluno(int i) {
  estadoAlunos[i].ativo = false;
  estadoAlunos[i].aluno = i;
  estadoAlunos[i].seq = -1;
  estadoAlunos[i].ciclo = -1;
  estadoAlunos[i].fc = -1;
  estadoAlunos[i].spo2 = -1;
  estadoAlunos[i].sys = -1;
  estadoAlunos[i].dia = -1;
  estadoAlunos[i].ir = -1;
  estadoAlunos[i].red = -1;
  estadoAlunos[i].ppg = "";
  estadoAlunos[i].ppgN = 0;
  estadoAlunos[i].ppgMs = 0;
  estadoAlunos[i].dedo = -1;
  estadoAlunos[i].qual = "NA";
  estadoAlunos[i].uso = "NA";
  estadoAlunos[i].sinalPpg = "NA";
  estadoAlunos[i].paValida = 0;
  estadoAlunos[i].movimento = "NA";
  estadoAlunos[i].artefatoPpg = "NA";
  estadoAlunos[i].rssi = 0;
  estadoAlunos[i].snr = 0.0f;
  estadoAlunos[i].batAluno = -1.0f;
  estadoAlunos[i].energiaProfessor = -1.0f;
  estadoAlunos[i].ack = 0;
  estadoAlunos[i].ultimoMs = 0;
}

static void inicializarEstadosApi() {
  for (int i = 0; i < 3; i++) {
    limparEstadoAluno(i);
  }

  estadoAlunos[1].aluno = 1;
  estadoAlunos[2].aluno = 2;
}

// ============================================================
// JSON
// ============================================================

static bool alunoAtivoAgora(const EstadoAlunoAPI& e) {
  if (e.ultimoMs == 0) {
    return false;
  }

  return (millis() - e.ultimoMs) <= API_ALUNO_ATIVO_TIMEOUT_MS;
}

static String jsonAluno(const EstadoAlunoAPI& e) {
  String json = "{";
  bool primeiro = true;

  bool ativoAgora = alunoAtivoAgora(e);
  campoBool(json, primeiro, "ativo", ativoAgora);
  campoInt(json, primeiro, "aluno", e.aluno);
  campoIntNA(json, primeiro, "seq", e.seq);
  campoIntNA(json, primeiro, "ciclo", e.ciclo);
  campoIntNA(json, primeiro, "fc", e.fc);
  campoIntNA(json, primeiro, "spo2", e.spo2);
  campoIntNA(json, primeiro, "sys", e.sys);
  campoIntNA(json, primeiro, "dia", e.dia);
  if (e.sys > 0 && e.dia > 0) {
    campoTexto(json, primeiro, "pa_est", String(e.sys) + "x" + String(e.dia));
  } else {
    campoTexto(json, primeiro, "pa_est", "NA");
  }
  campoLongNA(json, primeiro, "ir", e.ir);
  campoLongNA(json, primeiro, "red", e.red);

  campoPpgArray(json, primeiro, e.ppg);
  campoInt(json, primeiro, "ppg_n", e.ppgN);

  if (e.ppgMs > 0) {
    campoULong(json, primeiro, "ppg_idade_ms", millis() - e.ppgMs);
    campoULong(json, primeiro, "ppg_tempo_ms", e.ppgMs);
  } else {
    separador(json, primeiro);
    json += aspas("ppg_idade_ms");
    json += ':';
    json += "null";

    separador(json, primeiro);
    json += aspas("ppg_tempo_ms");
    json += ':';
    json += "null";
  }

  campoIntNA(json, primeiro, "dedo", e.dedo);
  campoTexto(json, primeiro, "qual", e.qual);
  campoTexto(json, primeiro, "uso", e.uso);
  campoTexto(json, primeiro, "sinal_ppg", e.sinalPpg);
  campoInt(json, primeiro, "pa_valida", e.paValida);
  campoTexto(json, primeiro, "movimento", e.movimento);
  campoTexto(json, primeiro, "artefato_ppg", e.artefatoPpg);
  campoInt(json, primeiro, "rssi", e.rssi);
  campoFloat(json, primeiro, "snr", e.snr, 2);
  campoFloatNA(json, primeiro, "bat_aluno", e.batAluno, 2);
  campoFloatNA(json, primeiro, "energia_professor", e.energiaProfessor, 2);
  campoInt(json, primeiro, "ack", e.ack);

  if (e.ultimoMs > 0) {
    campoULong(json, primeiro, "idade_ms", millis() - e.ultimoMs);
    campoULong(json, primeiro, "tempo_ms", e.ultimoMs);
  } else {
    separador(json, primeiro);
    json += aspas("idade_ms");
    json += ':';
    json += "null";

    separador(json, primeiro);
    json += aspas("tempo_ms");
    json += ':';
    json += "null";
  }

  json += '}';
  return json;
}


// ============================================================
// WIFI / ADMIN / PROVISIONAMENTO
// ============================================================

static String htmlEscape(const String& valor) {
  String s = valor;
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

static String modoWifiTexto() {
  if (wifiModoSetup) return "AP_SETUP";
  if (WiFi.status() == WL_CONNECTED) return "STA";
  return "DESCONECTADO";
}

static String ipAtualTexto() {
  if (wifiModoSetup) return WiFi.softAPIP().toString();
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return "0.0.0.0";
}

static bool conectarWiFiStation(const ConfigWiFi& cfg) {
  wifiModoSetup = false;

  if (!cfg.configurado) {
    Serial.println("Wi-Fi ainda nao configurado na NVS.");
    return false;
  }

  wifiSsidAtual = cfg.ssid;

  Serial.println("Iniciando API Professor em modo Wi-Fi STA...");
  Serial.print("Rede salva: ");
  Serial.println(cfg.ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(cfg.ssid.c_str(), cfg.senha.c_str());

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_TEMPO_CONEXAO_MS) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Falha ao conectar na rede salva.");
    return false;
  }

  Serial.print("Wi-Fi conectado em: ");
  Serial.println(cfg.ssid);
  Serial.print("IP STA: ");
  Serial.println(WiFi.localIP());

  return true;
}

static String montarJsonRedesDoScan(int n, int& totalValido) {
  String json = "{";
  json += "\"redes\":[";

  bool primeiro = true;
  totalValido = 0;

  if (n > 0) {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) {
        continue;
      }

      if (!primeiro) {
        json += ',';
      }
      primeiro = false;
      totalValido++;

      json += "{";
      json += "\"ssid\":";
      json += aspas(ssid);
      json += ",\"rssi\":";
      json += String(WiFi.RSSI(i));
      json += ",\"canal\":";
      json += String(WiFi.channel(i));
      json += ",\"aberta\":";
      json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "true" : "false";
      json += "}";
    }
  }

  json += "]}";
  return json;
}

static void executarScanWifi(const char* origem) {
  Serial.print("API admin: varredura Wi-Fi iniciada por ");
  Serial.println(origem);

  publicarEstadoScan(true, "Buscando redes Wi-Fi proximas...");

  if (wifiModoSetup) {
    // Mantem o SoftAP e habilita a interface station para permitir scan.
    // A pagina ja recebeu resposta antes da varredura; se houver uma oscilacao curta,
    // o navegador apenas tentara consultar o status novamente.
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_STA);
  }

  WiFi.setSleep(false);
  delay(100);

  int n = WiFi.scanNetworks(false, true);
  int totalValido = 0;
  String json = montarJsonRedesDoScan(n, totalValido);

  WiFi.scanDelete();

  String msg;
  if (n < 0) {
    msg = "Falha ao buscar redes. Tente novamente ou use SSID manual.";
    totalValido = 0;
    json = "{\"redes\":[]}";
  } else if (totalValido == 0) {
    msg = "Nenhuma rede encontrada. Aproxime o Professor do roteador ou digite manualmente.";
  } else {
    msg = String(totalValido) + " rede(s) encontrada(s).";
  }

  publicarCacheScan(json, totalValido, msg);

  Serial.print("API admin: varredura concluida. Redes validas: ");
  Serial.println(totalValido);
}

static void atualizarCacheRedesWifi() {
  executarScanWifi("inicializacao");
}

static void tarefaScanWifi(void* parametro) {
  (void)parametro;

  for (;;) {
    bool executar = false;

    garantirMutexScanWifi();
    if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (wifiScanSolicitado && !wifiScanEmAndamento) {
        wifiScanSolicitado = false;
        executar = true;
      }
      xSemaphoreGive(mutexScanWifi);
    }

    if (executar) {
      executarScanWifi("/api/scan/atualizar");
    }

    vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVALO_TAREFA_MS));
  }
}

static void iniciarWiFiSetupAP() {
  wifiModoSetup = true;
  wifiSsidAtual = WIFI_AP_SETUP_SSID;

  Serial.println("Iniciando rede fallback para configuracao Wi-Fi...");

  // A busca de redes e feita antes de ativar o SoftAP.
  // Isso evita ERR_NETWORK_CHANGED/timeout no navegador durante a varredura.
  atualizarCacheRedesWifi();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  bool ok = WiFi.softAP(WIFI_AP_SETUP_SSID, WIFI_AP_SETUP_SENHA);

  if (ok) {
    Serial.println("Rede de configuracao ativa.");
    Serial.print("SSID: ");
    Serial.println(WIFI_AP_SETUP_SSID);
    Serial.print("Senha: ");
    Serial.println(WIFI_AP_SETUP_SENHA);
    Serial.print("Acesse: http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("Pagina admin: /api/admin");
  } else {
    Serial.println("Falha ao iniciar SoftAP de configuracao.");
  }
}

static void responderAdmin() {
  ConfigWiFi cfg = carregarConfigWiFi();

  String html = R"ICNPADMIN(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Admin Wi-Fi ICNP</title>
<style>
  *{box-sizing:border-box}
  body{margin:0;font-family:Arial,Helvetica,sans-serif;background:#080c12;color:#eaf1fb}
  .wrap{max-width:1120px;margin:0 auto;padding:14px}
  .layout{display:grid;grid-template-columns:1fr 1fr;gap:12px;align-items:start}
  .card{background:#101823;border:1px solid #27374c;border-radius:15px;padding:14px}
  h1{margin:0 0 6px;font-size:24px}
  .sub{color:#9daec5;font-size:14px;margin-bottom:10px}
  label{display:block;font-weight:700;margin:10px 0 6px}
  input,select{width:100%;padding:11px;border-radius:10px;border:1px solid #32435a;background:#0d131c;color:#eaf1fb;font-size:15px}
  button,.btn{display:inline-block;background:#143824;border:1px solid #2d8a59;color:#7bf0a7;border-radius:10px;padding:10px 14px;font-weight:700;cursor:pointer;text-decoration:none;margin-top:8px}
  .danger{background:#4a1b23;border-color:#8d3342;color:#ff9cab}
  .muted{color:#9daec5;font-size:13px;margin-top:7px}
  .ok{color:#7bf0a7}.warn{color:#ffd36b}
  code{background:#0d131c;border:1px solid #263448;border-radius:6px;padding:2px 5px}
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .k{color:#9daec5;font-size:12px}.v{font-weight:700;margin-top:3px}
  .full{grid-column:1 / -1}
  @media(max-width:860px){.layout{grid-template-columns:1fr}.wrap{padding:10px}h1{font-size:22px}}
</style>
</head>
<body>
<div class="wrap">
  <div class="layout">
    <div class="card">
      <h1>Admin Wi-Fi do Professor ICNP</h1>
      <div class="sub">Configure a rede Wi-Fi do ambiente sem recompilar o firmware.</div>
      <div class="grid">
        <div><div class="k">Modo atual</div><div class="v">__MODO__</div></div>
        <div><div class="k">IP atual</div><div class="v">__IP__</div></div>
        <div><div class="k">SSID salvo</div><div class="v">__SSID_ATUAL__</div></div>
        <div><div class="k">Dashboard</div><div class="v"><a class="btn" href="/">Abrir painel</a></div></div>
      </div>
    </div>

    <div class="card">
      <label>Redes Wi-Fi proximas</label>
      <div style="display:flex;gap:8px;flex-wrap:wrap">
        <button type="button" onclick="buscarRedes()">Carregar lista</button>
        <button type="button" onclick="atualizarRedes()">Atualizar lista</button>
      </div>
      <div id="scanStatus" class="muted">Use a lista em cache ou atualize sem reiniciar o Professor.</div>

      <label>Selecionar rede encontrada</label>
      <select id="redesEncontradas" onchange="usarRedeSelecionada()">
        <option value="">Clique em carregar lista</option>
      </select>
    </div>

    <div class="card">
      <form method="POST" action="/api/config">
        <label>Nome da rede Wi-Fi (SSID)</label>
        <input id="ssid" name="ssid" maxlength="64" placeholder="Ex.: MinhaRede" value="__SSID_VALOR__" required>

        <label>Senha da rede Wi-Fi</label>
        <input name="senha" type="password" maxlength="64" placeholder="Senha da rede">

        <button type="submit">Salvar rede Wi-Fi</button>
      </form>
      <p class="muted">Ao salvar, o Professor reinicia e tenta entrar na rede configurada em modo station.</p>
    </div>

    <div class="card">
      <form method="POST" action="/api/config/apagar" onsubmit="return confirm('Apagar a rede salva?');">
        <button class="danger" type="submit">Apagar configuracao salva</button>
      </form>
      <p class="sub">Rede fallback: <code>ICNP_PROFESSOR_SETUP</code>. IP padrao: <code>192.168.4.1</code>.</p>
      <p class="muted">Se a rede desejada nao aparecer, clique em <b>Atualizar lista</b> ou digite o SSID manualmente.</p>
    </div>
  </div>
</div>

<script>
async function buscarRedes(){
  const status = document.getElementById('scanStatus');
  const select = document.getElementById('redesEncontradas');

  status.textContent = 'Carregando lista salva no Professor...';
  status.className = 'muted warn';
  select.innerHTML = '<option value="">Carregando...</option>';

  try {
    const resp = await fetch('/api/scan?t=' + Date.now());
    if (!resp.ok) throw new Error('HTTP ' + resp.status);

    const dados = await resp.json();
    select.innerHTML = '';

    if (!dados.redes || dados.redes.length === 0) {
      select.innerHTML = '<option value="">Nenhuma rede encontrada</option>';
      status.textContent = 'Nenhuma rede em cache. Clique em Atualizar lista ou digite manualmente.';
      status.className = 'muted warn';
      return;
    }

    select.appendChild(new Option('Selecione uma rede...', ''));

    dados.redes.forEach(function(r){
      const segura = r.aberta ? 'aberta' : 'segura';
      const texto = r.ssid + ' | RSSI ' + r.rssi + ' dBm | ' + segura;
      select.appendChild(new Option(texto, r.ssid));
    });

    status.textContent = dados.redes.length + ' rede(s) carregada(s). Selecione uma para preencher o SSID automaticamente.';
    status.className = 'muted ok';
  } catch(e) {
    select.innerHTML = '<option value="">Falha ao carregar redes</option>';
    status.textContent = 'Falha ao carregar a lista. Tente atualizar ou digite o SSID manualmente.';
    status.className = 'muted warn';
  }
}

async function atualizarRedes(){
  const status = document.getElementById('scanStatus');
  status.textContent = 'Solicitando nova varredura. Aguarde alguns segundos...';
  status.className = 'muted warn';

  try {
    const resp = await fetch('/api/scan/atualizar?t=' + Date.now(), {method:'POST'});
    if (!resp.ok) throw new Error('HTTP ' + resp.status);
    aguardarScan();
  } catch(e) {
    status.textContent = 'Falha ao iniciar a varredura. Digite o SSID manualmente ou tente novamente.';
    status.className = 'muted warn';
  }
}

async function aguardarScan(){
  const status = document.getElementById('scanStatus');

  try {
    const resp = await fetch('/api/scan/status?t=' + Date.now());
    if (!resp.ok) throw new Error('HTTP ' + resp.status);
    const dados = await resp.json();

    if (dados.escaneando) {
      status.textContent = 'Varredura em andamento...';
      status.className = 'muted warn';
      setTimeout(aguardarScan, 1000);
      return;
    }

    status.textContent = dados.mensagem || 'Varredura finalizada.';
    status.className = dados.total > 0 ? 'muted ok' : 'muted warn';
    buscarRedes();
  } catch(e) {
    // Durante a varredura o AP pode oscilar por instantes. Tentar novamente e manter a pagina viva.
    status.textContent = 'Aguardando resposta do Professor...';
    status.className = 'muted warn';
    setTimeout(aguardarScan, 1500);
  }
}

function usarRedeSelecionada(){
  const select = document.getElementById('redesEncontradas');
  if (select.value) {
    document.getElementById('ssid').value = select.value;
  }
}
</script>
</body>
</html>
)ICNPADMIN";

  String ssidVisivel = cfg.configurado ? cfg.ssid : "NAO CONFIGURADO";

  html.replace("__MODO__", htmlEscape(modoWifiTexto()));
  html.replace("__IP__", htmlEscape(ipAtualTexto()));
  html.replace("__SSID_ATUAL__", htmlEscape(ssidVisivel));
  html.replace("__SSID_VALOR__", cfg.configurado ? htmlEscape(cfg.ssid) : "");

  servidor.send(200, "text/html; charset=utf-8", html);
}

static void responderScanWifi() {
  Serial.println("API admin: enviando cache de redes Wi-Fi.");

  servidor.sendHeader("Access-Control-Allow-Origin", "*");
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json", obterCacheScan());
}

static void responderAtualizarScanWifi() {
  garantirMutexScanWifi();

  bool jaEscaneando = false;

  if (xSemaphoreTake(mutexScanWifi, pdMS_TO_TICKS(100)) == pdTRUE) {
    jaEscaneando = wifiScanEmAndamento;
    if (!wifiScanEmAndamento) {
      wifiScanSolicitado = true;
      wifiScanMensagem = "Nova varredura solicitada.";
    }
    xSemaphoreGive(mutexScanWifi);
  }

  String json = "{";
  bool primeiro = true;
  campoBool(json, primeiro, "iniciado", !jaEscaneando);
  campoBool(json, primeiro, "escaneando", jaEscaneando || !jaEscaneando);
  campoTexto(json, primeiro, "mensagem", jaEscaneando ? "Varredura ja esta em andamento." : "Varredura solicitada.");
  json += "}";

  servidor.sendHeader("Access-Control-Allow-Origin", "*");
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json", json);
}

static void responderStatusScanWifi() {
  bool emAndamento = obterScanEmAndamento();
  int total = obterTotalCacheScan();
  String msg = obterMensagemScan();
  unsigned long idade = obterIdadeCacheScanMs();

  String json = "{";
  bool primeiro = true;
  campoBool(json, primeiro, "escaneando", emAndamento);
  campoInt(json, primeiro, "total", total);
  campoULong(json, primeiro, "idade_cache_ms", idade);
  campoTexto(json, primeiro, "mensagem", msg);
  json += "}";

  servidor.sendHeader("Access-Control-Allow-Origin", "*");
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json", json);
}

static void responderConfigJson() {
  ConfigWiFi cfg = carregarConfigWiFi();

  String json = "{";
  bool primeiro = true;

  campoTexto(json, primeiro, "modo", modoWifiTexto());
  campoTexto(json, primeiro, "ip", ipAtualTexto());
  campoBool(json, primeiro, "configurado", cfg.configurado);
  campoTexto(json, primeiro, "ssid", cfg.configurado ? cfg.ssid : "");
  campoBool(json, primeiro, "conectado", WiFi.status() == WL_CONNECTED);

  json += "}";

  servidor.sendHeader("Access-Control-Allow-Origin", "*");
  servidor.send(200, "application/json", json);
}

static void responderSalvarConfig() {
  if (!servidor.hasArg("ssid")) {
    servidor.send(400, "text/plain; charset=utf-8", "Campo SSID ausente.");
    return;
  }

  String ssid = servidor.arg("ssid");
  String senha = servidor.hasArg("senha") ? servidor.arg("senha") : "";
  ssid.trim();

  if (ssid.length() == 0) {
    servidor.send(400, "text/plain; charset=utf-8", "SSID vazio.");
    return;
  }

  if (!salvarConfigWiFi(ssid, senha)) {
    servidor.send(500, "text/plain; charset=utf-8", "Falha ao salvar configuracao Wi-Fi.");
    return;
  }

  String html = R"ICNPSALVO(
<!DOCTYPE html>
<html lang="pt-BR">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Wi-Fi salvo</title>
<style>body{font-family:Arial;background:#080c12;color:#eaf1fb;padding:28px} .box{max-width:680px;margin:auto;background:#101823;border:1px solid #27374c;border-radius:16px;padding:20px} code{background:#0d131c;border:1px solid #263448;border-radius:6px;padding:2px 5px}</style>
</head>
<body><div class="box">
<h1>Rede Wi-Fi salva</h1>
<p>SSID configurado: <code>__SSID__</code></p>
<p>O Professor sera reiniciado para tentar conectar em modo station.</p>
<p>Se a senha estiver errada ou a rede nao estiver disponivel, a rede fallback <code>ICNP_PROFESSOR_SETUP</code> sera aberta novamente.</p>
</div>
<script>setTimeout(function(){ fetch('/api/reiniciar').catch(function(){}); }, 1200);</script>
</body></html>
)ICNPSALVO";

  html.replace("__SSID__", htmlEscape(ssid));
  servidor.send(200, "text/html; charset=utf-8", html);

  delay(1500);
  ESP.restart();
}

static void responderApagarConfig() {
  apagarConfigWiFi();

  servidor.send(200, "text/plain; charset=utf-8", "Configuracao Wi-Fi apagada. Reiniciando...");
  delay(500);
  ESP.restart();
}

static void responderReiniciar() {
  servidor.send(200, "text/plain; charset=utf-8", "Reiniciando Professor ICNP...");
  delay(500);
  ESP.restart();
}

// ============================================================
// ENDPOINT /api/status
// ============================================================

static void responderStatus() {
  EstadoAlunoAPI copia1;
  EstadoAlunoAPI copia2;

  garantirMutexEstado();

  if (xSemaphoreTake(mutexEstado, pdMS_TO_TICKS(50)) == pdTRUE) {
    copia1 = estadoAlunos[1];
    copia2 = estadoAlunos[2];
    xSemaphoreGive(mutexEstado);
  } else {
    copia1 = estadoAlunos[1];
    copia2 = estadoAlunos[2];
  }

  String json = "{";
  bool primeiro = true;

  campoInt(json, primeiro, "professor", 1);
  campoTexto(json, primeiro, "sistema", "ICNP_PPG");
  campoTexto(json, primeiro, "api", "ativa");
  campoTexto(json, primeiro, "wifi", modoWifiTexto());
  campoTexto(json, primeiro, "ip", ipAtualTexto());
  campoULong(json, primeiro, "tempo_professor_ms", millis());

  separador(json, primeiro);
  json += aspas("alunos");
  json += ':';
  json += '[';
  json += jsonAluno(copia1);
  json += ',';
  json += jsonAluno(copia2);
  json += ']';

  json += '}';

  servidor.sendHeader("Access-Control-Allow-Origin", "*");
  servidor.send(200, "application/json", json);
}

// ============================================================
// PAGINA HTML
// ============================================================

static void responderPagina() {
  servidor.send(200, "text/html; charset=utf-8", PAGINA_HTML);
}

// ============================================================
// TAREFA API
// ============================================================

static void tarefaApi(void* parametro) {
  (void)parametro;

  for (;;) {
    servidor.handleClient();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ============================================================
// INICIALIZACAO API
// ============================================================

void iniciarApiProfessor() {
  if (apiIniciada) {
    return;
  }

  garantirMutexEstado();
  inicializarEstadosApi();

  iniciarConfigWiFi();

  ConfigWiFi cfg = carregarConfigWiFi();
  bool conectadoSta = conectarWiFiStation(cfg);

  if (!conectadoSta) {
    iniciarWiFiSetupAP();
  }

  servidor.on("/", responderPagina);
  servidor.on("/api/status", responderStatus);
  servidor.on("/api/admin", HTTP_GET, responderAdmin);
  servidor.on("/admin", HTTP_GET, responderAdmin);
  servidor.on("/api/scan", HTTP_GET, responderScanWifi);
  servidor.on("/api/scan/atualizar", HTTP_POST, responderAtualizarScanWifi);
  servidor.on("/api/scan/status", HTTP_GET, responderStatusScanWifi);
  servidor.on("/api/config", HTTP_GET, responderConfigJson);
  servidor.on("/api/config", HTTP_POST, responderSalvarConfig);
  servidor.on("/api/config/apagar", HTTP_POST, responderApagarConfig);
  servidor.on("/api/reiniciar", HTTP_GET, responderReiniciar);
  servidor.on("/api/reiniciar", HTTP_POST, responderReiniciar);

  servidor.begin();

  if (handleTaskScanWifi == NULL) {
    xTaskCreatePinnedToCore(
      tarefaScanWifi,
      "tarefa_scan_wifi",
      4096,
      NULL,
      1,
      &handleTaskScanWifi,
      0
    );
  }

  apiIniciada = true;

  Serial.println("API Professor iniciada.");
  Serial.print("Modo Wi-Fi: ");
  Serial.println(modoWifiTexto());
  Serial.print("IP: ");
  Serial.println(ipAtualTexto());
  Serial.println("Wi-Fi sleep: OFF");
  Serial.println("Potencia Wi-Fi: 8.5 dBm");

  xTaskCreatePinnedToCore(
    tarefaApi,
    "tarefa_api_professor",
    8192,
    NULL,
    1,
    NULL,
    0
  );
}

// ============================================================
// ATUALIZACAO DO ESTADO
// ============================================================

void atualizarEstadoAlunoAPI(
  int aluno,
  int seq,
  int ciclo,
  int fc,
  int spo2,
  int sys,
  int dia,
  long ir,
  long red,
  int dedo,
  const String& qual,
  const String& uso,
  const String& sinalPpg,
  int paValida,
  const String& movimento,
  const String& artefatoPpg,
  int rssi,
  float snr,
  float batAluno,
  float energiaProfessor,
  int ack
) {
  if (aluno < 1 || aluno > 2) {
    return;
  }

  garantirMutexEstado();

  if (xSemaphoreTake(mutexEstado, pdMS_TO_TICKS(20)) == pdTRUE) {
    estadoAlunos[aluno].ativo = true;
    estadoAlunos[aluno].aluno = aluno;
    estadoAlunos[aluno].seq = seq;
    estadoAlunos[aluno].ciclo = ciclo;
    estadoAlunos[aluno].fc = fc;
    estadoAlunos[aluno].spo2 = spo2;
    estadoAlunos[aluno].sys = sys;
    estadoAlunos[aluno].dia = dia;
    estadoAlunos[aluno].ir = ir;
    estadoAlunos[aluno].red = red;
    estadoAlunos[aluno].dedo = dedo;
    estadoAlunos[aluno].qual = qual;
    estadoAlunos[aluno].uso = uso;
    estadoAlunos[aluno].sinalPpg = sinalPpg;
    estadoAlunos[aluno].paValida = paValida;
    estadoAlunos[aluno].movimento = movimento;
    estadoAlunos[aluno].artefatoPpg = artefatoPpg;
    estadoAlunos[aluno].rssi = rssi;
    estadoAlunos[aluno].snr = snr;
    estadoAlunos[aluno].batAluno = batAluno;
    estadoAlunos[aluno].energiaProfessor = energiaProfessor;
    estadoAlunos[aluno].ack = ack;
    estadoAlunos[aluno].ultimoMs = millis();

    xSemaphoreGive(mutexEstado);
  }
}

void atualizarPpgAlunoAPI(
  int aluno,
  const String& ppg,
  int ppgN
) {
  if (aluno < 1 || aluno > 2) {
    return;
  }

  garantirMutexEstado();

  if (xSemaphoreTake(mutexEstado, pdMS_TO_TICKS(20)) == pdTRUE) {
    EstadoAlunoAPI &e = estadoAlunos[aluno];

    // O pacote PPG nao renova sozinho o estado ativo.
    // A presenca online do Aluno e calculada a partir do ultimo DATA recebido.
    e.aluno = aluno;
    e.ppg = ppg;
    e.ppgN = ppgN;
    e.ppgMs = millis();

    xSemaphoreGive(mutexEstado);
  }
}
