/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * @file GfxBinding.cpp
 * @brief Squirrel bindings for gfx.* 2D canvas API.
 * @ref specs/Chapter 6 §6.3
 */

#include "GfxBinding.h"
#include "render/Canvas2D.h"
#include <sqstdaux.h>

namespace arcanee::script {

// Global canvas pointer set by Runtime before script execution
static render::Canvas2D *g_canvas = nullptr;

void setGfxCanvas(render::Canvas2D *canvas) { g_canvas = canvas; }

// ===== Clearing =====
static SQInteger gfx_clear(HSQUIRRELVM vm) {
  SQInteger color = 0x00000000;
  if (sq_gettop(vm) >= 2) {
    sq_getinteger(vm, 2, &color);
  }
  if (g_canvas) {
    g_canvas->clear(static_cast<u32>(color));
  }
  return 0;
}

// ===== State Stack =====
static SQInteger gfx_save(HSQUIRRELVM /*vm*/) {
  if (g_canvas)
    g_canvas->save();
  return 0;
}

static SQInteger gfx_restore(HSQUIRRELVM /*vm*/) {
  if (g_canvas)
    g_canvas->restore();
  return 0;
}

// ===== Transforms =====
static SQInteger gfx_resetTransform(HSQUIRRELVM /*vm*/) {
  if (g_canvas)
    g_canvas->resetTransform();
  return 0;
}

static SQInteger gfx_translate(HSQUIRRELVM vm) {
  SQFloat x, y;
  sq_getfloat(vm, 2, &x);
  sq_getfloat(vm, 3, &y);
  if (g_canvas)
    g_canvas->translate(x, y);
  return 0;
}

static SQInteger gfx_rotate(HSQUIRRELVM vm) {
  SQFloat rad;
  sq_getfloat(vm, 2, &rad);
  if (g_canvas)
    g_canvas->rotate(rad);
  return 0;
}

static SQInteger gfx_scale(HSQUIRRELVM vm) {
  SQFloat sx, sy;
  sq_getfloat(vm, 2, &sx);
  sq_getfloat(vm, 3, &sy);
  if (g_canvas)
    g_canvas->scale(sx, sy);
  return 0;
}

// ===== Styles =====
static SQInteger gfx_setFillColor(HSQUIRRELVM vm) {
  SQInteger color;
  sq_getinteger(vm, 2, &color);
  if (g_canvas)
    g_canvas->setFillColor(static_cast<u32>(color));
  return 0;
}

static SQInteger gfx_setStrokeColor(HSQUIRRELVM vm) {
  SQInteger color;
  sq_getinteger(vm, 2, &color);
  if (g_canvas)
    g_canvas->setStrokeColor(static_cast<u32>(color));
  return 0;
}

static SQInteger gfx_setLineWidth(HSQUIRRELVM vm) {
  SQFloat width;
  sq_getfloat(vm, 2, &width);
  if (g_canvas)
    g_canvas->setLineWidth(width);
  return 0;
}

static SQInteger gfx_setGlobalAlpha(HSQUIRRELVM vm) {
  SQFloat alpha;
  sq_getfloat(vm, 2, &alpha);
  if (g_canvas)
    g_canvas->setGlobalAlpha(alpha);
  return 0;
}

// ===== Paths =====
static SQInteger gfx_beginPath(HSQUIRRELVM /*vm*/) {
  if (g_canvas)
    g_canvas->beginPath();
  return 0;
}

static SQInteger gfx_closePath(HSQUIRRELVM /*vm*/) {
  if (g_canvas)
    g_canvas->closePath();
  return 0;
}

static SQInteger gfx_moveTo(HSQUIRRELVM vm) {
  SQFloat x, y;
  sq_getfloat(vm, 2, &x);
  sq_getfloat(vm, 3, &y);
  if (g_canvas)
    g_canvas->moveTo(x, y);
  return 0;
}

static SQInteger gfx_lineTo(HSQUIRRELVM vm) {
  SQFloat x, y;
  sq_getfloat(vm, 2, &x);
  sq_getfloat(vm, 3, &y);
  if (g_canvas)
    g_canvas->lineTo(x, y);
  return 0;
}

static SQInteger gfx_rect(HSQUIRRELVM vm) {
  SQFloat x, y, w, h;
  sq_getfloat(vm, 2, &x);
  sq_getfloat(vm, 3, &y);
  sq_getfloat(vm, 4, &w);
  sq_getfloat(vm, 5, &h);
  if (g_canvas)
    g_canvas->rect(x, y, w, h);
  return 0;
}

// ===== Drawing =====
static SQInteger gfx_fill(HSQUIRRELVM /*vm*/) {
  if (g_canvas)
    g_canvas->fill();
  return 0;
}

static SQInteger gfx_stroke(HSQUIRRELVM /*vm*/) {
  if (g_canvas)
    g_canvas->stroke();
  return 0;
}

static SQInteger gfx_fillRect(HSQUIRRELVM vm) {
  SQFloat x, y, w, h;
  sq_getfloat(vm, 2, &x);
  sq_getfloat(vm, 3, &y);
  sq_getfloat(vm, 4, &w);
  sq_getfloat(vm, 5, &h);
  if (g_canvas)
    g_canvas->fillRect(x, y, w, h);
  return 0;
}

static SQInteger gfx_strokeRect(HSQUIRRELVM vm) {
  SQFloat x, y, w, h;
  sq_getfloat(vm, 2, &x);
  sq_getfloat(vm, 3, &y);
  sq_getfloat(vm, 4, &w);
  sq_getfloat(vm, 5, &h);
  if (g_canvas)
    g_canvas->strokeRect(x, y, w, h);
  return 0;
}

static SQInteger gfx_getTargetSize(HSQUIRRELVM vm) {
  if (!g_canvas) {
    sq_pushinteger(vm, 0);
    sq_pushinteger(vm, 0);
    return 2;
  }
  sq_pushinteger(vm, g_canvas->getWidth());
  sq_pushinteger(vm, g_canvas->getHeight());
  return 2;
}

// ===== Registration =====
void registerGfxBinding(HSQUIRRELVM vm) {
  // Create gfx table
  sq_pushroottable(vm);
  sq_pushstring(vm, "gfx", -1);
  sq_newtable(vm);

  // Clearing
  sq_pushstring(vm, "clear", -1);
  sq_newclosure(vm, gfx_clear, 0);
  sq_newslot(vm, -3, SQFalse);

  // State stack
  sq_pushstring(vm, "save", -1);
  sq_newclosure(vm, gfx_save, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "restore", -1);
  sq_newclosure(vm, gfx_restore, 0);
  sq_newslot(vm, -3, SQFalse);

  // Transforms
  sq_pushstring(vm, "resetTransform", -1);
  sq_newclosure(vm, gfx_resetTransform, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "translate", -1);
  sq_newclosure(vm, gfx_translate, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "rotate", -1);
  sq_newclosure(vm, gfx_rotate, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "scale", -1);
  sq_newclosure(vm, gfx_scale, 0);
  sq_newslot(vm, -3, SQFalse);

  // Styles
  sq_pushstring(vm, "setFillColor", -1);
  sq_newclosure(vm, gfx_setFillColor, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "setStrokeColor", -1);
  sq_newclosure(vm, gfx_setStrokeColor, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "setLineWidth", -1);
  sq_newclosure(vm, gfx_setLineWidth, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "setGlobalAlpha", -1);
  sq_newclosure(vm, gfx_setGlobalAlpha, 0);
  sq_newslot(vm, -3, SQFalse);

  // Paths
  sq_pushstring(vm, "beginPath", -1);
  sq_newclosure(vm, gfx_beginPath, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "closePath", -1);
  sq_newclosure(vm, gfx_closePath, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "moveTo", -1);
  sq_newclosure(vm, gfx_moveTo, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "lineTo", -1);
  sq_newclosure(vm, gfx_lineTo, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "rect", -1);
  sq_newclosure(vm, gfx_rect, 0);
  sq_newslot(vm, -3, SQFalse);

  // Drawing
  sq_pushstring(vm, "fill", -1);
  sq_newclosure(vm, gfx_fill, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "stroke", -1);
  sq_newclosure(vm, gfx_stroke, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "fillRect", -1);
  sq_newclosure(vm, gfx_fillRect, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "strokeRect", -1);
  sq_newclosure(vm, gfx_strokeRect, 0);
  sq_newslot(vm, -3, SQFalse);

  sq_pushstring(vm, "getTargetSize", -1);
  sq_newclosure(vm, gfx_getTargetSize, 0);
  sq_newslot(vm, -3, SQFalse);

  // Add gfx table to root
  sq_newslot(vm, -3, SQFalse);
  sq_pop(vm, 1);
}

} // namespace arcanee::script
