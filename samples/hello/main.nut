// Hello ARCANEE - Sample Cartridge Entry Script
// Demonstrates basic cartridge structure per Chapter 4

// Cartridge state
local frameCount = 0;
local x = 100.0;
local y = 100.0;

// Called once at cartridge start
function init() {
    print("Hello ARCANEE! Initializing...");
    
    // Get console size
    // local size = sys.getConsoleSize();
    // print("Console size: " + size.w + "x" + size.h);
}

// Called at fixed timestep (60 Hz by default)
function update(dt) {
    frameCount++;
    
    // Simple movement
    x = x + 50.0 * dt;
    if (x > 800.0) {
        x = 0.0;
    }
}

// Called once per video frame with interpolation alpha
function draw(alpha) {
    // Clear screen
    // gfx.clear(0xFF000000);
    
    // Draw a simple rectangle
    // local drawX = x + (velocityX * alpha);  // Interpolated position
    // gfx.setColor(0xFF00FF00);
    // gfx.fillRect(drawX, y, 64, 64);
    
    // Draw text
    // gfx.setColor(0xFFFFFFFF);
    // gfx.drawText(10, 10, "Frame: " + frameCount);
    
    // Note: gfx.* API requires render2d module (Phase 5)
}
