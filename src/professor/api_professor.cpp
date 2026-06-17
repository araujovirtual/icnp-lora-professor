#include "api_professor.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "freertos/semphr.h"

// ============================================================
// CONFIGURACAO DO WIFI / API
// ============================================================

static const char* WIFI_SSID = "JUMAMIHEI_2";
static const char* WIFI_SENHA = "123mudarJXYZ";

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
  s += valor;
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
  campoTexto(json, primeiro, "wifi", "STA");
  campoTexto(json, primeiro, "ip", WiFi.localIP().toString());
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

  Serial.println("Iniciando API Professor em modo Wi-Fi STA...");
  Serial.print("Rede: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  WiFi.begin(WIFI_SSID, WIFI_SENHA);

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Falha ao conectar Wi-Fi. API nao iniciada.");
    return;
  }

  servidor.on("/", responderPagina);
  servidor.on("/api/status", responderStatus);
  servidor.begin();

  apiIniciada = true;

  Serial.println("API Professor iniciada em modo STA.");
  Serial.print("Wi-Fi conectado em: ");
  Serial.println(WIFI_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
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