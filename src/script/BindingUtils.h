#pragma once

#include <squirrel.h>
#include <string>

namespace arcanee::script {

/**
 * @brief Helper used to bind a native Global or Member function to a Squirrel
 * VM. Pushes the function as a closure into the table currently at the top of
 * the stack.
 */
inline void BindFunction(HSQUIRRELVM vm, const char *name, SQFUNCTION func) {
  sq_pushstring(vm, name, -1);
  sq_newclosure(vm, func, 0); // 0 free variables
  sq_newslot(vm, -3,
             SQFalse); // Push into table at -3 (stack: table, key, closure)
}

/**
 * @brief Templated argument retriever.
 * Must be specialized for supported types.
 */
template <typename T> SQRESULT GetArg(HSQUIRRELVM vm, SQInteger idx, T &out) {
  (void)vm;
  (void)idx;
  (void)out;
  return SQ_ERROR; // Placeholder
}

template <>
inline SQRESULT GetArg<SQInteger>(HSQUIRRELVM vm, SQInteger idx,
                                  SQInteger &out) {
  return sq_getinteger(vm, idx, &out);
}

// Conflict happens if int == SQInteger, but usually safe to specialize distinct
// types if they differ If SQInteger is long long and int is int, they differ.
template <>
inline SQRESULT GetArg<int>(HSQUIRRELVM vm, SQInteger idx, int &out) {
  SQInteger val;
  if (SQ_FAILED(sq_getinteger(vm, idx, &val)))
    return SQ_ERROR;
  out = static_cast<int>(val);
  return SQ_OK;
}

template <>
inline SQRESULT GetArg<SQFloat>(HSQUIRRELVM vm, SQInteger idx, SQFloat &out) {
  return sq_getfloat(vm, idx, &out);
}

// Only enable if float != SQFloat to avoid redefinition
#ifdef SQUIRREL_FLOAT_IS_NOT_FLOAT
template <>
inline SQRESULT GetArg<float>(HSQUIRRELVM vm, SQInteger idx, float &out) {
  SQFloat val;
  if (SQ_FAILED(sq_getfloat(vm, idx, &val)))
    return SQ_ERROR;
  out = static_cast<float>(val);
  return SQ_OK;
}
#endif

// Only enable if double != SQFloat
#ifdef SQUIRREL_FLOAT_IS_NOT_DOUBLE
template <>
inline SQRESULT GetArg<double>(HSQUIRRELVM vm, SQInteger idx, double &out) {
  SQFloat val;
  if (SQ_FAILED(sq_getfloat(vm, idx, &val)))
    return SQ_ERROR;
  out = static_cast<double>(val);
  return SQ_OK;
}
#endif

template <>
inline SQRESULT GetArg<const SQChar *>(HSQUIRRELVM vm, SQInteger idx,
                                       const SQChar *&out) {
  return sq_getstring(vm, idx, &out);
}

template <>
inline SQRESULT GetArg<std::string>(HSQUIRRELVM vm, SQInteger idx,
                                    std::string &out) {
  const SQChar *str = nullptr;
  if (SQ_FAILED(sq_getstring(vm, idx, &str)))
    return SQ_ERROR;
  out = str;
  return SQ_OK;
}

template <>
inline SQRESULT GetArg<bool>(HSQUIRRELVM vm, SQInteger idx, bool &out) {
  SQBool val;
  if (SQ_FAILED(sq_getbool(vm, idx, &val)))
    return SQ_ERROR;
  out = (val != SQFalse);
  return SQ_OK;
}

} // namespace arcanee::script
