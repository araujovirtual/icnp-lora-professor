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
  .tv .graficos{padding:6px;border-right:none;border-bottom:1px solid #27374c;display:grid;grid-template-rows:1fr 1fr;gap:6px;min-height:0}
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
    <div class="sub">Graficos com esteira temporal continua: o tempo entra pela direita e sai pela esquerda.</div>
  </div>

  <div>
    <span class="sub">Alunos por tela: </span>
    <button class="btn" id="b1" onclick="setCols(1)">1</button>
    <button class="btn" id="b2" onclick="setCols(2)">2</button>
    <button class="btn" id="b3" onclick="setCols(3)">3</button>
    <button class="btn" id="b4" onclick="setCols(4)">4</button>
    <button class="btn" onclick="full()">Tela cheia</button>
  </div>
</div>

<div class="page">
  <div id="status"></div>
  <div id="monitores" class="monitores cols-2"></div>
  <div class="rodape">Tendencias reais por ciclo ICNP. Nao ha ECG real nem forma de onda PPG continua.</div>
</div>

<script>
let estado={alunos:[]};
let cols=Number(localStorage.getItem('icnp_cols')||'2');
let hist={};
let animacaoIniciada=false;
let ultimoFrameDesenho=0;

const JANELA_MS=60000;
const PASSO_TEMPO_MS=5000;
const INTERVALO_API_MS=1000;
const INTERVALO_DESENHO_MS=120;

function H(id){
  if(!hist[id]) hist[id]={ultimoRegistroTs:0,p:[],max:240};
  return hist[id];
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
  return a && a.ativo===true && a.dedo===1 && a.qual==='OK';
}

function okFc(a){
  return okBase(a) && a.fc!==null && a.fc!==undefined && Number(a.fc)>0;
}

function okSpo2(a){
  return okBase(a) && a.spo2!==null && a.spo2!==undefined && Number(a.spo2)>0;
}

function okBat(a){
  return a && a.bat_aluno!==null && a.bat_aluno!==undefined && Number(a.bat_aluno)>0;
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

  let corFcGrande=okFc(a)?corFCValor(a.fc):'#718096';
  let corSpGrande=okSpo2(a)?corSpO2Valor(a.spo2):'#718096';
  let corBatGrande=okBat(a)?corBatValor(a.bat_aluno):'#718096';

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
            '<span class="fc">FC: tempo x bpm</span>'+
            '<span style="color:'+corFcGrande+'">'+(okFc(a)?a.fc+' bpm':'sem FC valida')+'</span>'+
          '</div>'+
          '<canvas id="fc'+id+'"></canvas>'+
        '</div>'+

        '<div class="graf">'+
          '<div class="linha">'+
            '<span class="spo2">SpO2: tempo x %</span>'+
            '<span style="color:'+corSpGrande+'">'+(okSpo2(a)?a.spo2+' %':'sem SpO2 valida')+'</span>'+
          '</div>'+
          '<canvas id="sp'+id+'"></canvas>'+
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
          '<div class="box"><div class="k">Ciclo/Seq</div><div class="v">'+t(a.ciclo)+'/'+t(a.seq)+'</div></div>'+
          '<div class="box"><div class="k">Bat Aluno</div><div class="v" style="color:'+corBatGrande+'">'+t(a.bat_aluno,' V')+'</div></div>'+
          '<div class="box"><div class="k">Energia Prof</div><div class="v">'+t(a.energia_professor,' V')+'</div></div>'+
          '<div class="box"><div class="k">RSSI/SNR</div><div class="v">'+t(a.rssi)+'/'+t(a.snr)+'</div></div>'+
          '<div class="box"><div class="k">Idade</div><div class="v">'+t(a.idade_ms,' ms')+'</div></div>'+
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
  let L=44,T=12,R=12,B=38;
  let PW=w-L-R;
  let PH=h-T-B;

  let agora=Date.now();
  let t1=agora;
  let t0=agora-JANELA_MS;

  let valid=p.filter(q=>{
    return q &&
           q.ts &&
           q.ts>=t0 &&
           q.ts<=t1 &&
           q[campo]!==null &&
           q[campo]!==undefined;
  });

  if(valid.length<2){
    sem(id,msg);
    return;
  }

  x.clearRect(0,0,w,h);
  grade(x,w,h,L,T,R,B);

  // ==========================================================
  // EIXO Y
  // ==========================================================

  x.fillStyle='#9daec5';
  x.font='10px Arial';
  x.textAlign='right';
  x.textBaseline='middle';

  for(let i=0;i<=4;i++){
    let val=maxY-(maxY-minY)*i/4;
    let y=T+PH*i/4;
    x.fillText(String(Math.round(val*10)/10),L-6,y);
  }

  x.save();
  x.translate(10,T+PH/2);
  x.rotate(-Math.PI/2);
  x.textAlign='center';
  x.fillText(un,0,0);
  x.restore();

  // ==========================================================
  // ESTEIRA DO TEMPO NO EIXO X
  // ==========================================================

  let primeiroTick=Math.floor(t0/PASSO_TEMPO_MS)*PASSO_TEMPO_MS;
  let ultimoTick=t1+PASSO_TEMPO_MS;

  for(let tick=primeiroTick;tick<=ultimoTick;tick+=PASSO_TEMPO_MS){
    let xx=L+((tick-t0)/(t1-t0))*PW;

    if(xx>=L && xx<=L+PW){
      x.strokeStyle='rgba(255,255,255,0.13)';
      x.lineWidth=1;
      x.beginPath();
      x.moveTo(xx,T);
      x.lineTo(xx,T+PH);
      x.stroke();

      x.strokeStyle='#9daec5';
      x.beginPath();
      x.moveTo(xx,T+PH);
      x.lineTo(xx,T+PH+4);
      x.stroke();
    }
  }

  x.save();
  x.beginPath();
  x.rect(L, T+PH+4, PW, B-2);
  x.clip();

  x.fillStyle='#9daec5';
  x.font='10px Arial';
  x.textAlign='center';
  x.textBaseline='top';

  for(let tick=primeiroTick;tick<=ultimoTick;tick+=PASSO_TEMPO_MS){
    let xx=L+((tick-t0)/(t1-t0))*PW;

    if(xx>=L-45 && xx<=L+PW+45){
      x.fillText(horaDeTimestamp(tick),xx,T+PH+10);
    }
  }

  x.restore();

  x.strokeStyle='rgba(255,255,255,0.28)';
  x.lineWidth=1;
  x.beginPath();
  x.moveTo(L+PW,T);
  x.lineTo(L+PW,T+PH);
  x.stroke();

  // ==========================================================
  // LINHA DO GRAFICO COLORIDA POR FAIXA OPERACIONAL
  // ==========================================================

  x.lineWidth=2;

  let anterior=null;

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
      x.beginPath();
      x.moveTo(anterior.x,anterior.y);
      x.lineTo(xx,yy);
      x.stroke();
    }

    x.fillStyle=corAtual;
    x.beginPath();
    x.arc(xx,yy,2.6,0,Math.PI*2);
    x.fill();

    anterior={x:xx,y:yy};
  }

  // ==========================================================
  // ROTULOS DE VALOR A CADA 5 s
  // ==========================================================

  let ocupados=[];

  ocupados.push({x:w-145,y:0,w:145,h:26});

  for(let tick=primeiroTick;tick<=ultimoTick;tick+=PASSO_TEMPO_MS){
    let tickX=L+((tick-t0)/(t1-t0))*PW;

    if(tickX<L+36 || tickX>L+PW-36) continue;

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
    let pontoY=T+PH-((vv-minY)/(maxY-minY))*PH;
    let textoValor=String(valorOriginal);

    let candidatos=[
      {x:tickX,    y:pontoY-8},
      {x:tickX,    y:pontoY-20},
      {x:tickX-24, y:pontoY-8},
      {x:tickX+24, y:pontoY-8},
      {x:tickX,    y:pontoY+20},
      {x:tickX-30, y:pontoY+20},
      {x:tickX+30, y:pontoY+20}
    ];

    let escolhido=null;
    let escolhidoRect=null;

    for(let c=0;c<candidatos.length;c++){
      let cand=candidatos[c];

      if(cand.y>T+PH-20) cand.y=T+PH-22;
      if(cand.y<T+16) cand.y=T+18;

      let rect=textoRetanguloEstimado(textoValor,cand.x,cand.y);

      if(podeDesenhar(rect,ocupados,L,T,PW,PH)){
        escolhido=cand;
        escolhidoRect=rect;
        break;
      }
    }

    if(escolhido && escolhidoRect){
      x.fillStyle='rgba(11,17,25,0.82)';
      x.fillRect(escolhidoRect.x-2,escolhidoRect.y-1,escolhidoRect.w+4,escolhidoRect.h+2);

      x.fillStyle=corPorCampo(campo,valorOriginal,corPadrao);
      x.font='bold 10px Arial';
      x.textAlign='center';
      x.textBaseline='bottom';
      x.fillText(textoValor,escolhido.x,escolhido.y);

      ocupados.push({
        x:escolhidoRect.x-4,
        y:escolhidoRect.y-3,
        w:escolhidoRect.w+8,
        h:escolhidoRect.h+6
      });
    }
  }

  // ==========================================================
  // ULTIMO VALOR
  // ==========================================================

  let u=ultVisivel(p,campo,t0,t1);

  if(u!==null){
    x.fillStyle=corPorCampo(campo,u,corPadrao);
    x.font='11px Arial';
    x.textAlign='right';
    x.textBaseline='top';
    x.fillText('Ultimo: '+u+' '+un,w-8,5);
  }
}

function drawAll(){
  if(!estado.alunos) return;

  estado.alunos.forEach(a=>{
    let p=H(a.aluno).p;

    graf('fc'+a.aluno,p,'fc','#6ff26d',40,180,'bpm','sem historico de FC valido');
    graf('sp'+a.aluno,p,'spo2','#70d8ff',80,100,'%','sem historico de SpO2 valido');
    graf('bt'+a.aluno,p,'bat','#ffd15c',3.0,4.2,'V','sem historico de bateria');
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

    renderStatus(d);

    d.alunos.forEach(addHist);

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
  estadoAlunos[i].ir = -1;
  estadoAlunos[i].red = -1;
  estadoAlunos[i].dedo = -1;
  estadoAlunos[i].qual = "NA";
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

static String jsonAluno(const EstadoAlunoAPI& e) {
  String json = "{";
  bool primeiro = true;

  campoBool(json, primeiro, "ativo", e.ativo);
  campoInt(json, primeiro, "aluno", e.aluno);
  campoIntNA(json, primeiro, "seq", e.seq);
  campoIntNA(json, primeiro, "ciclo", e.ciclo);
  campoIntNA(json, primeiro, "fc", e.fc);
  campoIntNA(json, primeiro, "spo2", e.spo2);
  campoLongNA(json, primeiro, "ir", e.ir);
  campoLongNA(json, primeiro, "red", e.red);
  campoIntNA(json, primeiro, "dedo", e.dedo);
  campoTexto(json, primeiro, "qual", e.qual);
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
  long ir,
  long red,
  int dedo,
  const String& qual,
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
    estadoAlunos[aluno].ir = ir;
    estadoAlunos[aluno].red = red;
    estadoAlunos[aluno].dedo = dedo;
    estadoAlunos[aluno].qual = qual;
    estadoAlunos[aluno].rssi = rssi;
    estadoAlunos[aluno].snr = snr;
    estadoAlunos[aluno].batAluno = batAluno;
    estadoAlunos[aluno].energiaProfessor = energiaProfessor;
    estadoAlunos[aluno].ack = ack;
    estadoAlunos[aluno].ultimoMs = millis();

    xSemaphoreGive(mutexEstado);
  }
}