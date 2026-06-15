#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer server(80);

// ВСЯ ВЕБ-СТРАНИЦА В ОДНОЙ ПЕРЕМЕННОЙ PROGMEM
const char GAME_CODE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP GameBox</title>
    <style>
        * { box-sizing: border-box; user-select: none; -webkit-user-select: none; touch-action: none; }
        body { margin: 0; padding: 0; background: #1a1a1a; color: #fff; font-family: 'Courier New', monospace; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; overflow: hidden; }
        #menu { display: flex; flex-direction: column; gap: 12px; text-align: center; width: 90%; max-width: 400px; }
        h1 { color: #5cb85c; text-shadow: 2px 2px #000; font-size: 28px; margin-bottom: 20px; }
        .btn { padding: 16px; font-size: 18px; font-weight: bold; background: #34495e; border: 3px solid #2c3e50; color: #ecf0f1; border-radius: 8px; cursor: pointer; box-shadow: 0 4px #1a252f; }
        .btn:active { transform: translateY(4px); box-shadow: 0 0px #1a252f; }
        #game-container { display: none; position: relative; width: 100vw; height: 100vh; background: #000; }
        #close-btn { position: absolute; top: 10px; right: 10px; width: 45px; height: 45px; background: #e74c3c; border: 2px solid #fff; color: white; font-size: 28px; font-weight: bold; border-radius: 50%; cursor: pointer; z-index: 999; display: flex; align-items: center; justify-content: center; box-shadow: 0 3px 6px rgba(0,0,0,0.5); }
        #game-canvas { display: block; background: #111; margin: 0 auto; max-width: 100%; max-height: 65vh; border-bottom: 4px solid #333; }
        #controls { position: absolute; bottom: 0; left: 0; width: 100%; height: 32vh; background: #222; display: grid; grid-template-columns: repeat(3, 1fr); padding: 10px; gap: 10px; box-sizing: border-box; }
        .ctrl-btn { background: #444; border: 2px solid #666; color: #fff; font-size: 20px; font-weight: bold; border-radius: 12px; display: flex; align-items: center; justify-content: center; box-shadow: 0 4px #222; -webkit-tap-highlight-color: transparent; }
        .ctrl-btn.active-press { background: #666; transform: translateY(2px); box-shadow: 0 2px #222; }
        #mc-inv { position: absolute; top: 10px; left: 10px; display: none; gap: 5px; background: rgba(0,0,0,0.7); padding: 5px; border-radius: 5px; z-index: 100; }
        .inv-slot { width: 38px; height: 38px; border: 2px solid #555; display: flex; align-items: center; justify-content: center; font-size: 11px; font-weight: bold; border-radius: 4px; background: #333; cursor: pointer; }
        .inv-slot.active { border-color: #5cb85c; background: #444; }
    </style>
</head>
<body>
    <div id="menu">
        <h1>🕹️ ESP-GAMEBOX</h1>
        <button class="btn" onclick="switchGame('tetris')">TETRIS</button>
        <button class="btn" onclick="switchGame('mario')">Dinosair</button>
        <button class="btn" onclick="switchGame('geometry')">Geometry Dash</button>
        <button class="btn" onclick="switchGame('minecraft')">Minecraft 2D</button>
    </div>

    <div id="game-container">
        <button id="close-btn" onclick="exitToMenu()">×</button>
        <div id="mc-inv"></div>
        <canvas id="game-canvas"></canvas>
        <div id="controls"></div>
    </div>
)=====";
const char JS_CORE[] PROGMEM = R"=====(
<script>
const menu = document.getElementById('menu');
const container = document.getElementById('game-container');
const canvas = document.getElementById('game-canvas');
const ctx = canvas.getContext('2d');
const controls = document.getElementById('controls');
const invBar = document.getElementById('mc-inv');

let currentGame = null;
let gameLoop = null;
let keys = {};

function switchGame(gameName) {
    menu.style.display = 'none';
    container.style.display = 'block';
    invBar.style.display = (gameName === 'minecraft') ? 'flex' : 'none';
    
    if(gameLoop) clearInterval(gameLoop);
    keys = {};
    currentGame = gameName;
    
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight * 0.65;
    
    setupControls(gameName);
    
    if (gameName === 'tetris') initTetris();
    else if (gameName === 'mario') initMario();
    else if (gameName === 'geometry') initGeometry();
    else if (gameName === 'minecraft') initMinecraft();
}

function exitToMenu() {
    if(gameLoop) clearInterval(gameLoop);
    currentGame = null;
    container.style.display = 'none';
    menu.style.display = 'flex';
    invBar.style.display = 'none';
    ctx.clearRect(0, 0, canvas.width, canvas.height);
}

function bindBtn(id, keyName) {
    const btn = document.getElementById(id);
    if (!btn) return;
    const startPress = (e) => { e.preventDefault(); keys[keyName] = true; btn.classList.add('active-press'); };
    const endPress = (e) => { e.preventDefault(); keys[keyName] = false; btn.classList.remove('active-press'); };
    btn.addEventListener('touchstart', startPress, {passive: false});
    btn.addEventListener('touchend', endPress, {passive: false});
    btn.addEventListener('mousedown', startPress);
    btn.addEventListener('mouseup', endPress);
    btn.addEventListener('mouseleave', endPress);
}

function setupControls(game) {
    controls.innerHTML = '';
    if (game === 'tetris') {
        controls.innerHTML = `
            <button class="ctrl-btn" id="btn-l">←</button>
            <button class="ctrl-btn" id="btn-rot">↻</button>
            <button class="ctrl-btn" id="btn-r">→</button>
            <div></div><button class="ctrl-btn" id="btn-d">↓</button><div></div>`;
        bindBtn('btn-l', 'Left'); bindBtn('btn-rot', 'Rot'); bindBtn('btn-r', 'Right'); bindBtn('btn-d', 'Down');
    } else if (game === 'mario') {
        controls.innerHTML = `
            <button class="ctrl-btn" id="btn-l">←</button>
            <button class="ctrl-btn" id="btn-j">Jump</button>
            <button class="ctrl-btn" id="btn-r">→</button>`;
        bindBtn('btn-l', 'Left'); bindBtn('btn-j', 'Jump'); bindBtn('btn-r', 'Right');
    } else if (game === 'geometry') {
        controls.innerHTML = `<button class="ctrl-btn" id="btn-j" style="grid-column: span 3; height: 90%;">Click to jump</button>`;
        bindBtn('btn-j', 'Jump');
    } else if (game === 'minecraft') {
        controls.innerHTML = `
            <button class="ctrl-btn" id="btn-l">←</button>
            <button class="ctrl-btn" id="btn-j">▲</button>
            <button class="ctrl-btn" id="btn-r">→</button>
            <button class="ctrl-btn" id="btn-brk">break</button>
            <button class="ctrl-btn" id="btn-bld">place</button>
            <button class="ctrl-btn" id="btn-hit"> attack⚔️</button>`;
        bindBtn('btn-l', 'Left'); bindBtn('btn-j', 'Jump'); bindBtn('btn-r', 'Right');
        bindBtn('btn-brk', 'Break'); bindBtn('btn-bld', 'Build'); bindBtn('btn-hit', 'Hit');
    }
}

window.addEventListener('keydown', e => {
    if(e.key==="ArrowLeft" || e.key==="a") keys['Left']=true;
    if(e.key==="ArrowRight" || e.key==="d") keys['Right']=true;
    if(e.key==="ArrowUp" || e.key==="w" || e.key===" ") { keys['Jump']=true; keys['Rot']=true; }
    if(e.key==="ArrowDown" || e.key==="s") keys['Down']=true;
});
window.addEventListener('keyup', e => {
    if(e.key==="ArrowLeft" || e.key==="a") keys['Left']=false;
    if(e.key==="ArrowRight" || e.key==="d") keys['Right']=false;
    if(e.key==="ArrowUp" || e.key==="w" || e.key===" ") { keys['Jump']=false; keys['Rot']=false; }
    if(e.key==="ArrowDown" || e.key==="s") keys['Down']=false;
});
</script>
)=====";
const char JS_GAMES_1[] PROGMEM = R"=====(
<script>
// === 1. ТЕТРИС ===
function initTetris() {
    const COLS = 10, ROWS = 20, SIZE = Math.min(canvas.width/COLS, canvas.height/ROWS);
    let board = Array(ROWS).fill().map(() => Array(COLS).fill(0));
    let piece = { matrix: [[1,1],[1,1]], x: 4, y: 0 }, tick = 0;
    
    gameLoop = setInterval(() => {
        ctx.fillStyle = '#111'; ctx.fillRect(0,0,canvas.width,canvas.height);
        
        if(keys['Left'] && piece.x > 0) { piece.x--; keys['Left']=false; }
        if(keys['Right'] && piece.x < COLS-piece.matrix.length) { piece.x++; keys['Right']=false; }
        if(keys['Rot']) {
            piece.matrix = piece.matrix.map((_,i) => piece.matrix.map(r => r[i]).reverse());
            keys['Rot']=false;
        }
        
        tick++;
        if(tick % 12 === 0 || keys['Down']) {
            piece.y++;
            if(piece.y + piece.matrix.length > ROWS) {
                piece.y--;
                piece.matrix.forEach((r,i)=>r.forEach((v,j)=>{ if(v) board[piece.y+i][piece.x+j]=1; }));
                board = board.filter(r => !r.every(v => v));
                while(board.length < ROWS) board.unshift(Array(COLS).fill(0));
                piece = { matrix: [[1,1],[1,1]], x: 4, y: 0 };
            }
        }
        
        board.forEach((r,y)=>r.forEach((v,x)=>{ if(v) { ctx.fillStyle='#444'; ctx.fillRect(x*SIZE,y*SIZE,SIZE-1,SIZE-1); } }));
        piece.matrix.forEach((r,i)=>r.forEach((v,j)=>{ if(v) { ctx.fillStyle='#e74c3c'; ctx.fillRect((piece.x+j)*SIZE,(piece.y+i)*SIZE,SIZE-1,SIZE-1); } }));
    }, 50);
}

// === 2. SUPER MARIO ===
function initMario() {
    let mario = { x: 50, y: 0, vy: 0, r: 12, grounded: true }, obstacles = [], score = 0, speed = 4;
    gameLoop = setInterval(() => {
        ctx.fillStyle = '#5c94fc'; ctx.fillRect(0,0,canvas.width,canvas.height);
        ctx.fillStyle = '#fcb43c'; ctx.fillRect(0, canvas.height-40, canvas.width, 40);
        
        if(keys['Jump'] && mario.grounded) { mario.vy = -12; mario.grounded = false; keys['Jump']=false; }
        mario.vy += 0.6; mario.y += mario.vy;
        if(mario.y >= canvas.height - 40 - mario.r) { mario.y = canvas.height - 40 - mario.r; mario.vy = 0; mario.grounded = true; }
        
        if(keys['Left']) mario.x = Math.max(12, mario.x - 4);
        if(keys['Right']) mario.x = Math.min(canvas.width - 12, mario.x + 4);
        
        speed += 0.002; score++;
        if(Math.random() < 0.015 && obstacles.length < 3) obstacles.push({ x: canvas.width, w: 20, h: 30 + Math.random()*20 });
        
        ctx.fillStyle = '#ff4444'; ctx.fillRect(mario.x - mario.r, mario.y - mario.r, mario.r*2, mario.r*2);
        
        ctx.fillStyle = '#00a800';
        obstacles.forEach((o) => {
            o.x -= speed;
            ctx.fillRect(o.x, canvas.height - 40 - o.h, o.w, o.h);
            if(mario.x + mario.r > o.x && mario.x - mario.r < o.x + o.w && mario.y + mario.r > canvas.height - 40 - o.h) {
                alert("Game OVER! " + score); initMario();
            }
        });
        obstacles = obstacles.filter(o => o.x > -20);
        ctx.fillStyle = '#fff'; ctx.font = '16px Arial'; ctx.fillText('Hard/Power: ' + Math.floor(score), 10, 25);
    }, 1000/60);
}

// === 3. GEOMETRY DASH ===
function initGeometry() {
    let player = { y: 0, vy: 0, size: 25, rot: 0 }, spikes = [], speed = 5, score = 0;
    gameLoop = setInterval(() => {
        ctx.fillStyle = '#000022'; ctx.fillRect(0,0,canvas.width,canvas.height);
        ctx.fillStyle = '#00ff00'; ctx.fillRect(0, canvas.height-30, canvas.width, 30);
        
        if(keys['Jump'] && player.y === 0) { player.vy = -10; keys['Jump'] = false; }
        player.vy += 0.5; player.y += player.vy;
        if(player.y >= 0) { player.y = 0; player.vy = 0; player.rot = 0; }
        else { player.rot += 0.1; }
        
        speed += 0.001; score++;
        if(Math.random() < 0.01 && spikes.length < 2) spikes.push({ x: canvas.width });
        
        ctx.save();
        ctx.translate(80, canvas.height - 30 - player.y - player.size/2);
        ctx.rotate(player.rot);
        ctx.fillStyle = '#00ffff'; ctx.fillRect(-player.size/2, -player.size/2, player.size, player.size);
        ctx.restore();
        
        ctx.fillStyle = '#ff0055';
        spikes.forEach((s) => {
            s.x -= speed;
            ctx.beginPath(); ctx.moveTo(s.x, canvas.height-30); ctx.lineTo(s.x+15, canvas.height-60); ctx.lineTo(s.x+30, canvas.height-30); ctx.fill();
            if(s.x < 95 && s.x + 30 > 65 && player.y < 30) {
                alert("reset! :power " + score); initGeometry();
            }
        });
        spikes = spikes.filter(s => s.x > -30);
    }, 1000/60);
}
</script>
)=====";
const char JS_MINECRAFT[] PROGMEM = R"=====(
<script>
let inv = { wood: 0, dirt: 0, stone: 0, bed: 0, workbench: 0, pickaxe: 1, sword: 1 };
let activeSlot = 6;

function setSlot(num) {
    activeSlot = num;
    for(let i=1; i<=7; i++) {
        const el = document.getElementById('slot-'+i);
        if(el) el.className = 'inv-slot' + (i===num?' active':'');
    }
}

function updateInvUI() {
    document.getElementById('slot-1').innerText = '🪵' + inv.wood;
    document.getElementById('slot-2').innerText = '🟫' + inv.dirt;
    document.getElementById('slot-3').innerText = '🧱' + inv.stone;
    document.getElementById('slot-4').innerText = '🛏️' + inv.bed;
    document.getElementById('slot-5').innerText = '📦' + inv.workbench;
    document.getElementById('slot-6').innerText = '⛏️';
    document.getElementById('slot-7').innerText = '⚔️';
}

function drawBlockTexture(ctx, type, x, y, size) {
    if (type === 1) {
        ctx.fillStyle = '#593a23'; ctx.fillRect(x, y, size, size);
        ctx.fillStyle = '#3d2514'; ctx.fillRect(x, y+4, size, 4); ctx.fillRect(x, y+14, size, 4); ctx.fillRect(x, y+24, size, 4);
    } else if (type === 2) {
        ctx.fillStyle = '#866043'; ctx.fillRect(x, y, size, size);
        ctx.fillStyle = '#578b31'; ctx.fillRect(x, y, size, 8);
        ctx.fillStyle = '#4c7828'; ctx.fillRect(x, y+6, size, 2);
    } else if (type === 3) {
        ctx.fillStyle = '#737373'; ctx.fillRect(x, y, size, size);
        ctx.fillStyle = '#5a5a5a'; ctx.fillRect(x+4, y+4, 6, 4); ctx.fillRect(x+16, y+12, 8, 4); ctx.fillRect(x+8, y+22, 6, 4);
    } else if (type === 4) {
        ctx.fillStyle = '#ff2222'; ctx.fillRect(x, y+10, size, size-10);
        ctx.fillStyle = '#ffffff'; ctx.fillRect(x, y+2, 10, 8);
    } else if (type === 5) {
        ctx.fillStyle = '#3b6e22'; ctx.fillRect(x, y, size, size);
        ctx.fillStyle = '#284e16'; ctx.fillRect(x+2, y+4, 8, 8); ctx.fillRect(x+16, y+14, 10, 10);
    } else if (type === 8) {
        ctx.fillStyle = '#b38059'; ctx.fillRect(x, y, size, size);
        ctx.fillStyle = '#593a23'; ctx.strokeRect(x+2, y+2, size-4, size-4);
        ctx.fillStyle = '#000000'; ctx.fillRect(x+6, y+8, 4, 4); ctx.fillRect(x+20, y+8, 4, 4);
    }
}

function initMinecraft() {
    const BS = 32;
    const COLS = Math.ceil(canvas.width / BS), ROWS = Math.ceil(canvas.height / BS);
    let world = Array(ROWS).fill().map(() => Array(COLS).fill(0));
    let player = { x: 2 * BS, y: 3 * BS, vx:0, vy:0, w: 22, h: 32, hp: 10, dir: 1 };
    let zombies = [], time = 0;
    
    invBar.innerHTML = `
        <div id="slot-1" class="inv-slot" onclick="setSlot(1)">🪵0</div>
        <div id="slot-2" class="inv-slot" onclick="setSlot(2)">🟫0</div>
        <div id="slot-3" class="inv-slot" onclick="setSlot(3)">🧱0</div>
        <div id="slot-4" class="inv-slot" onclick="setSlot(4)">🛏️0</div>
        <div id="slot-5" class="inv-slot" onclick="setSlot(5)">📦0</div>
        <div id="slot-6" class="inv-slot active" onclick="setSlot(6)">⛏️</div>
        <div id="slot-7" class="inv-slot" onclick="setSlot(7)">⚔️</div>
    `;
    updateInvUI();

    for(let x=0; x<COLS; x++) {
        let ground = Math.floor(ROWS/2) + Math.floor(Math.sin(x*0.4)*2);
        if (ground >= ROWS) ground = ROWS - 1;
        world[ground][x] = 2;
        for(let y=ground+1; y<ROWS; y++) world[y][x] = (y>ground+2 && Math.random()>0.25) ? 3 : 2;
        if(x % 7 === 3 && ground > 4) {
            world[ground-1][x] = 1; world[ground-2][x] = 1; world[ground-3][x] = 1;
            world[ground-4][x] = 5; world[ground-4][x-1] = 5; world[ground-4][x+1] = 5; world[ground-5][x] = 5;
        }
    }

    gameLoop = setInterval(() => {
        time = (time + 1) % 2000;
        let isNight = time > 1000;
        
        if(keys['Left']) { player.vx = -3; player.dir = -1; }
        else if(keys['Right']) { player.vx = 3; player.dir = 1; }
        else player.vx = 0;
        
        if(keys['Jump'] && player.vy === 0) player.vy = -7.5;
        player.vy += 0.4; player.x += player.vx; player.y += player.vy;
        
        let px = Math.floor((player.x + player.w/2)/BS), py = Math.floor((player.y + player.h)/BS);
        if(py >= 0 && py < ROWS && px >= 0 && px < COLS && world[py][px] > 0 && world[py][px]!==5) {
            player.y = py * BS - player.h; player.vy = 0;
        }

        if(inv.wood >= 3) { inv.wood -= 3; inv.workbench += 1; updateInvUI(); }
        if(inv.wood >= 2 && inv.stone >= 2 && inv.workbench >= 1) { inv.wood -= 2; inv.stone -= 2; inv.bed += 1; updateInvUI(); }

        let bx = Math.floor((player.x + (player.dir === 1 ? player.w + 12 : -12)) / BS);
        let by = Math.floor((player.y + player.h/2) / BS);
        
        if(keys['Break']) {
            if(activeSlot === 6) {
                if(bx>=0 && bx<COLS && by>=0 && by<ROWS) {
                    let t = world[by][bx];
                    if(t > 0) {
                        if(t===1) inv.wood++; if(t===2) inv.dirt++; if(t===3) inv.stone++; 
                        if(t===4) inv.bed++; if(t===8) inv.workbench++;
                        world[by][bx] = 0; updateInvUI();
                    }
                }
            }
            keys['Break'] = false;
        }

        if(keys['Build']) {
            if(bx>=0 && bx<COLS && by>=0 && by<ROWS && world[by][bx]===0) {
                if(activeSlot===1 && inv.wood>0) { world[by][bx]=1; inv.wood--; }
                else if(activeSlot===2 && inv.dirt>0) { world[by][bx]=2; inv.dirt--; }
                else if(activeSlot===3 && inv.stone>0) { world[by][bx]=3; inv.stone--; }
                else if(activeSlot===4 && inv.bed>0) { world[by][bx]=4; inv.bed--; if(isNight) time = 0; }
                else if(activeSlot===5 && inv.workbench>0) { world[by][bx]=8; inv.workbench--; }
                updateInvUI();
            }
            keys['Build'] = false; 
        }

        if(keys['Hit'] && bx>=0 && bx<COLS && by>=0 && by<ROWS && world[by][bx]===8) {
            alert("🛠️ STEAVE open and helting!");
            player.hp = 10; keys['Hit'] = false;
        }

        if(isNight && Math.random() < 0.02 && zombies.length < 3) zombies.push({x: canvas.width, y: player.y, hp: 3});
        
        if(keys['Hit']) {
            let dmg = (activeSlot === 7) ? 1.5 : 0.4;
            zombies.forEach(z => { if(Math.abs(z.x - player.x) < 65) { z.hp -= dmg; z.x += player.dir * 20; } });
            keys['Hit'] = false;
        }

        ctx.fillStyle = isNight ? '#0c0c26' : '#5c94fc'; ctx.fillRect(0,0,canvas.width,canvas.height);
        
        for(let y=0; y<ROWS; y++) {
            for(let x=0; x<COLS; x++) { if(world[y][x] > 0) drawBlockTexture(ctx, world[y][x], x*BS, y*BS, BS); }
        }

        let headSize = 8;
        ctx.fillStyle = '#ffcd94'; ctx.fillRect(player.x+6, player.y, headSize, headSize);
        ctx.fillStyle = '#00bfff'; ctx.fillRect(player.x+4, player.y+headSize, player.w-8, 12);
        ctx.fillStyle = '#0000cd'; ctx.fillRect(player.x+4, player.y+headSize+12, player.w-8, 12);
        
        ctx.save();
        ctx.translate(player.x + (player.dir === 1 ? player.w - 2 : 2), player.y + 14);
        ctx.fillStyle = '#ffcd94'; ctx.fillRect(0, 0, player.dir * 8, 4);
        ctx.font = '12px Arial';
        if(activeSlot === 1) ctx.fillText('🪵', player.dir*6, 6);
        if(activeSlot === 2) ctx.fillText('🟫', player.dir*6, 6);
        if(activeSlot === 3) ctx.fillText('🧱', player.dir*6, 6);
        if(activeSlot === 4) ctx.fillText('🛏️', player.dir*6, 6);
        if(activeSlot === 5) ctx.fillText('📦', player.dir*6, 6);
        if(activeSlot === 6) ctx.fillText('⛏️', player.dir*6, 6);
        if(activeSlot === 7) ctx.fillText('⚔️', player.dir*6, 6);
        ctx.restore();
        
        zombies.forEach((z) => {
            z.x += (player.x > z.x) ? 1.0 : -1.0;
            ctx.fillStyle = '#5a823e'; ctx.fillRect(z.x+6, z.y, 8, 8);
            ctx.fillStyle = '#29577a'; ctx.fillRect(z.x+4, z.y+8, 12, 12);
            ctx.fillStyle = '#3c4043'; ctx.fillRect(z.x+4, z.y+20, 12, 12);
            ctx.fillStyle = '#5a823e'; ctx.fillRect(z.x + (player.x > z.x ? 14 : -6), z.y+10, 8, 4);
            if(Math.abs(z.x - player.x) < 18 && Math.abs(z.y - player.y) < 20) player.hp -= 0.12;
        });
        zombies = zombies.filter(z => z.hp > 0);

        if(player.hp <= 0) { alert("You Died!"); exitToMenu(); }
        
        ctx.fillStyle='#fff'; ctx.font='bold 14px monospace';
        ctx.fillText('❤️ HP: ' + Math.ceil(player.hp) + ' | ' + (isNight ? '🌙 Night' : '☀️ 'DAY), 115, 25);
    }, 1000/30);
}
</script>
)=====";
const char HTML_FOOT[] PROGMEM = "</body></html>";

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  
  // Последовательно выплескиваем все куски из памяти в один поток
  server.sendContent_P(GAME_CODE);
  server.sendContent_P(JS_CORE);
  server.sendContent_P(JS_GAMES_1);
  server.sendContent_P(JS_MINECRAFT);
  server.sendContent_P(HTML_FOOT);
  
  server.sendContent("");
}

void setup() {
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Game");

  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", handleRoot);
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://192.168.4"), true);
    server.send(302, "text/plain", ""); 
  });
  
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
