// Hello World Sample
local foo = require("foo.nut");

::print("Main loaded. Foo says: " + foo.message + "\n");

function update(dt) {
    foo.update(dt);
}

function draw(alpha) {
}
