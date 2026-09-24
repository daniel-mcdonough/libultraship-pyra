#include "ship/resource/BenchLookup.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "ship/Context.h"

std::shared_ptr<Ship::CVar> CVarGet(const char* name) {
    BENCH_CVAR_SCOPE("CVarGet");
    return Ship::Context::GetRawInstance()->GetConsoleVariables()->Get(name);
}

extern "C" {
int32_t CVarGetInteger(const char* name, int32_t defaultValue) {
    BENCH_CVAR_SCOPE("CVarGetInteger");
    return Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger(name, defaultValue);
}

float CVarGetFloat(const char* name, float defaultValue) {
    BENCH_CVAR_SCOPE("CVarGetFloat");
    return Ship::Context::GetRawInstance()->GetConsoleVariables()->GetFloat(name, defaultValue);
}

const char* CVarGetString(const char* name, const char* defaultValue) {
    BENCH_CVAR_SCOPE("CVarGetString");
    return Ship::Context::GetRawInstance()->GetConsoleVariables()->GetString(name, defaultValue);
}

Color_RGBA8 CVarGetColor(const char* name, Color_RGBA8 defaultValue) {
    BENCH_CVAR_SCOPE("CVarGetColor");
    return Ship::Context::GetRawInstance()->GetConsoleVariables()->GetColor(name, defaultValue);
}

Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 defaultValue) {
    return Ship::Context::GetRawInstance()->GetConsoleVariables()->GetColor24(name, defaultValue);
}

void CVarSetInteger(const char* name, int32_t value) {
    BENCH_CVAR_WRITE_SCOPE("CVarSetInteger");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetInteger(name, value);
}

void CVarSetFloat(const char* name, float value) {
    BENCH_CVAR_WRITE_SCOPE("CVarSetFloat");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetFloat(name, value);
}

void CVarSetString(const char* name, const char* value) {
    BENCH_CVAR_WRITE_SCOPE("CVarSetString");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetString(name, value);
}

void CVarSetColor(const char* name, Color_RGBA8 value) {
    BENCH_CVAR_WRITE_SCOPE("CVarSetColor");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetColor(name, value);
}

void CVarSetColor24(const char* name, Color_RGB8 value) {
    BENCH_CVAR_WRITE_SCOPE("CVarSetColor24");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->SetColor24(name, value);
}

void CVarRegisterInteger(const char* name, int32_t defaultValue) {
    BENCH_CVAR_WRITE_SCOPE("CVarRegisterInteger");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->RegisterInteger(name, defaultValue);
}

void CVarRegisterFloat(const char* name, float defaultValue) {
    BENCH_CVAR_WRITE_SCOPE("CVarRegisterFloat");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->RegisterFloat(name, defaultValue);
}

void CVarRegisterString(const char* name, const char* defaultValue) {
    BENCH_CVAR_WRITE_SCOPE("CVarRegisterString");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->RegisterString(name, defaultValue);
}

void CVarRegisterColor(const char* name, Color_RGBA8 defaultValue) {
    BENCH_CVAR_WRITE_SCOPE("CVarRegisterColor");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->RegisterColor(name, defaultValue);
}

void CVarRegisterColor24(const char* name, Color_RGB8 defaultValue) {
    BENCH_CVAR_WRITE_SCOPE("CVarRegisterColor24");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->RegisterColor24(name, defaultValue);
}

void CVarClear(const char* name) {
    BENCH_CVAR_WRITE_SCOPE("CVarClear");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->ClearVariable(name);
}

void CVarClearBlock(const char* name) {
    BENCH_CVAR_WRITE_SCOPE("CVarClearBlock");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->ClearBlock(name);
}

void CVarCopy(const char* from, const char* to) {
    BENCH_CVAR_WRITE_SCOPE("CVarCopy");
    Ship::Context::GetRawInstance()->GetConsoleVariables()->CopyVariable(from, to);
}

void CVarLoad() {
    Ship::Context::GetRawInstance()->GetConsoleVariables()->Load();
}

void CVarSave() {
    Ship::Context::GetRawInstance()->GetConsoleVariables()->Save();
}
}
