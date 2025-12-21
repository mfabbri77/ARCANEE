#include "InputBinding.h"
#include "app/Runtime.h"
#include "common/Log.h"
#include "input/InputManager.h"

namespace arcanee::script {

static input::InputManager *getInputManager() {
  // TODO: Add static accessor to Runtime or InputManager directly?
  // Runtime has it.
  // For now, we can follow the pattern of AudioManager where we have a global
  // setter/getter or access via Runtime if global runtime exists. AudioManager
  // used a static global. Let's add that to InputManager for simplicity in
  // bindings.
  return input::getInputManager();
}

// inp.btn(scancode)
static SQInteger inp_btn(HSQUIRRELVM vm) {
  SQInteger scancode;
  if (SQ_FAILED(sq_getinteger(vm, 2, &scancode)))
    return sq_throwerror(vm, "Invalid argument");

  if (auto *mgr = getInputManager()) {
    sq_pushbool(vm, mgr->isKeyDown((int)scancode) ? SQTrue : SQFalse);
    return 1;
  }
  sq_pushbool(vm, SQFalse);
  return 1;
}

// inp.btnp(scancode)
static SQInteger inp_btnp(HSQUIRRELVM vm) {
  SQInteger scancode;
  if (SQ_FAILED(sq_getinteger(vm, 2, &scancode)))
    return sq_throwerror(vm, "Invalid argument");

  if (auto *mgr = getInputManager()) {
    sq_pushbool(vm, mgr->isKeyPressed((int)scancode) ? SQTrue : SQFalse);
    return 1;
  }
  sq_pushbool(vm, SQFalse);
  return 1;
}

// inp.mouse_x()
static SQInteger inp_mouse_x(HSQUIRRELVM vm) {
  if (auto *mgr = getInputManager()) {
    sq_pushinteger(vm, mgr->getCurrentSnapshot().mouse.x);
    return 1;
  }
  sq_pushinteger(vm, -1);
  return 1;
}

// inp.mouse_y()
static SQInteger inp_mouse_y(HSQUIRRELVM vm) {
  if (auto *mgr = getInputManager()) {
    sq_pushinteger(vm, mgr->getCurrentSnapshot().mouse.y);
    return 1;
  }
  sq_pushinteger(vm, -1);
  return 1;
}

// inp.mouse_btn(btn)
static SQInteger inp_mouse_btn(HSQUIRRELVM vm) {
  SQInteger btn;
  if (SQ_FAILED(sq_getinteger(vm, 2, &btn)))
    return sq_throwerror(vm, "Invalid argument");

  if (auto *mgr = getInputManager()) {
    sq_pushbool(vm, mgr->isMouseButtonDown((int)btn) ? SQTrue : SQFalse);
    return 1;
  }
  sq_pushbool(vm, SQFalse);
  return 1;
}

// inp.mouse_btnp(btn)
static SQInteger inp_mouse_btnp(HSQUIRRELVM vm) {
  SQInteger btn;
  if (SQ_FAILED(sq_getinteger(vm, 2, &btn)))
    return sq_throwerror(vm, "Invalid argument");

  if (auto *mgr = getInputManager()) {
    sq_pushbool(vm, mgr->isMouseButtonPressed((int)btn) ? SQTrue : SQFalse);
    return 1;
  }
  sq_pushbool(vm, SQFalse);
  return 1;
}

void registerInputBinding(HSQUIRRELVM vm) {
  sq_pushroottable(vm);
  sq_pushstring(vm, "inp", -1);
  sq_newtable(vm);

  auto registerFunc = [&](const char *name, SQFUNCTION func) {
    sq_pushstring(vm, name, -1);
    sq_newclosure(vm, func, 0);
    sq_newslot(vm, -3, SQFalse);
  };

  registerFunc("btn", inp_btn);
  registerFunc("btnp", inp_btnp);
  registerFunc("mouse_x", inp_mouse_x);
  registerFunc("mouse_y", inp_mouse_y);
  registerFunc("mouse_btn", inp_mouse_btn);
  registerFunc("mouse_btnp", inp_mouse_btnp);

  sq_newslot(vm, -3, SQFalse);
  sq_pop(vm, 1); // root
}

} // namespace arcanee::script
