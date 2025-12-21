// ARCANEE Audio Test - Music Playback & SFX

local t = 0.0;
local sfx1 = -1;
local sfx2 = -1;

function init() {
    print("Audio Test Initialized");
    
    // Load Music
    local h = audio.loadModule("cart:/music.xm");
    audio.playModule(h);
    
    // Load SFX (Assuming standard assets exist or will be generated)
    // If files are missing, these calls might fail/return -1
    sfx1 = audio.loadSound("cart:/sfx1.wav"); 
    sfx2 = audio.loadSound("cart:/sfx2.wav");
    
    print("SFX1 Handle: " + sfx1);
    print("SFX2 Handle: " + sfx2);
}

function update(dt) {
    t += dt;
    
    // Test SFX inputs (Standard mappings: 0=Z/Space, 1=X)
    if (inp.btnp(0)) {
        print("Btn 0 (Jump) -> Play SFX1");
        if (sfx1 != -1) audio.playSound(sfx1, 1.0, 0.0, false);
    }
    if (inp.btnp(1)) {
        print("Btn 1 (Fire) -> Play SFX2");
        if (sfx2 != -1) audio.playSound(sfx2, 1.0, 0.0, false);
    }
}

function draw(alpha) {
    // Pulse background matching tempo/time
    local val = (sin(t * 5.0) + 1.0) * 0.5; // 0..1
    
    // Clear screen
    if (val > 0.5) gfx.clear(2); else gfx.clear(3);
    
    local w = gfx.getTargetSize()[0];
    local h = gfx.getTargetSize()[1];

    // Center Box
    gfx.setFillColor(7);
    gfx.fillRect(w/2 - 50, h/2 - 50, 100, 100);
    
    // Visualize "Sound" Rings
    gfx.setStrokeColor(11);
    gfx.setLineWidth(5.0);
    for(local i=0; i<5; i+=1) {
        local r = 60 + (i * 20 * val);
        gfx.beginPath();
        gfx.moveTo(w/2, h/2 - r);
        gfx.lineTo(w/2 + r, h/2);
        gfx.lineTo(w/2, h/2 + r);
        gfx.lineTo(w/2 - r, h/2);
        gfx.closePath();
        gfx.stroke();
    }
    
    // Input Feedback
    if(inp.btn(0)) {
        gfx.setFillColor(8); // Red for button press
        gfx.fillRect(20, h-40, 20, 20);
    }
    if(inp.btn(1)) {
        gfx.setFillColor(12); // Blue for button press
        gfx.fillRect(50, h-40, 20, 20);
    }
}
