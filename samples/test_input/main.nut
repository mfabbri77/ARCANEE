// ARCANEE Input Test - Visualizer

function init() {
    print("Input Test Initialized");
}

function update(dt) {
    // No logic needed, direct polling in draw
}

function draw(alpha) {
    gfx.clear(1); // Dark Blue BG

    local w = gfx.getTargetSize()[0];
    local h = gfx.getTargetSize()[1];

    // Mouse Cursor
    local mx = inp.mouse_x();
    local my = inp.mouse_y();
    
    gfx.setStrokeColor(7); // White
    gfx.setLineWidth(2.0);
    gfx.beginPath();
    gfx.moveTo(mx - 10, my);
    gfx.lineTo(mx + 10, my);
    gfx.moveTo(mx, my - 10);
    gfx.lineTo(mx, my + 10);
    gfx.stroke();

    if (inp.mouse_btn(0)) {
        gfx.setFillColor(8); // Red click
        gfx.fillRect(mx + 5, my + 5, 10, 10);
    }
    
    // Gamepad Visualizer
    local cx = w / 2;
    local cy = h / 2;
    
    // Body
    gfx.setStrokeColor(6); // Gray
    gfx.setLineWidth(4.0);
    gfx.strokeRect(cx - 100, cy - 50, 200, 100);
    
    // D-Pad (Left)
    local dpadX = cx - 60;
    local dpadY = cy + 10;
    
    // Up (Btn 10)
    if (inp.btn(10)) gfx.setFillColor(11); else gfx.setFillColor(5);
    gfx.fillRect(dpadX, dpadY - 20, 10, 20);
    
    // Down (Btn 11)
    if (inp.btn(11)) gfx.setFillColor(11); else gfx.setFillColor(5);
    gfx.fillRect(dpadX, dpadY + 10, 10, 20);
    
    // Left (Btn 12)
    if (inp.btn(12)) gfx.setFillColor(11); else gfx.setFillColor(5);
    gfx.fillRect(dpadX - 20, dpadY, 20, 10);
    
    // Right (Btn 13)
    if (inp.btn(13)) gfx.setFillColor(11); else gfx.setFillColor(5);
    gfx.fillRect(dpadX + 10, dpadY, 20, 10);
    
    // Face Buttons (Right)
    local btnX = cx + 60;
    local btnY = cy + 10;
    
    // A (Btn 0)
    if (inp.btn(0)) gfx.setFillColor(11); else gfx.setFillColor(8);
    gfx.fillRect(btnX, btnY + 10, 10, 10); // Simulated circle
    
    // B (Btn 1)
    if (inp.btn(1)) gfx.setFillColor(11); else gfx.setFillColor(8);
    gfx.fillRect(btnX + 10, btnY, 10, 10);
    
    // X (Btn 2)
    if (inp.btn(2)) gfx.setFillColor(11); else gfx.setFillColor(8);
    gfx.fillRect(btnX - 10, btnY, 10, 10);
    
    // Y (Btn 3)
    if (inp.btn(3)) gfx.setFillColor(11); else gfx.setFillColor(8);
    gfx.fillRect(btnX, btnY - 10, 10, 10);
}
