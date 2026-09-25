#pragma once

#include "ship/utils/color.h"
#include <nlohmann/json.hpp>
#include <stdint.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Ship {
typedef enum class ConsoleVariableType { Integer, Float, String, Color, Color24 } ConsoleVariableType;

typedef struct CVar {
    ConsoleVariableType Type;
    union {
        int32_t Integer;
        float Float;
        char* String = nullptr;
        Color_RGBA8 Color;
        Color_RGB8 Color24;
    };
    ~CVar() {
        if (Type == ConsoleVariableType::String && String != nullptr) {
            free(String);
        }
    }
} CVar;

class ConsoleVariable {
  public:
    ConsoleVariable();
    ~ConsoleVariable();

    std::shared_ptr<CVar> Get(const char* name);

    int32_t GetInteger(const char* name, int32_t defaultValue);
    float GetFloat(const char* name, float defaultValue);
    const char* GetString(const char* name, const char* defaultValue);
    Color_RGBA8 GetColor(const char* name, Color_RGBA8 defaultValue);
    Color_RGB8 GetColor24(const char* name, Color_RGB8 defaultValue);

    void SetInteger(const char* name, int32_t value);
    void SetFloat(const char* name, float value);
    void SetString(const char* name, const char* value);
    void SetColor(const char* name, Color_RGBA8 value);
    void SetColor24(const char* name, Color_RGB8 value);

    void RegisterInteger(const char* name, int32_t defaultValue);
    void RegisterFloat(const char* name, float defaultValue);
    void RegisterString(const char* name, const char* defaultValue);
    void RegisterColor(const char* name, Color_RGBA8 defaultValue);
    void RegisterColor24(const char* name, Color_RGB8 defaultValue);

    void ClearVariable(const char* name);
    void ClearBlock(const char* name);
    void CopyVariable(const char* from, const char* to);

    void Save();
    void Load();

  protected:
    void LoadFromPath(std::string path,
                      nlohmann::detail::iteration_proxy<nlohmann::detail::iter_impl<nlohmann::json>> items);
    void LoadLegacy();

  private:
    // Transparent, so that a lookup by const char* need not build a std::string key (§8.31).
    // std::hash<std::string_view> is the hash std::hash<std::string> uses, so the table is the same.
    struct NameHash {
        using is_transparent = void;
        size_t operator()(std::string_view name) const {
            return std::hash<std::string_view>{}(name);
        }
    };
    std::unordered_map<std::string, std::shared_ptr<CVar>, NameHash, std::equal_to<>> mVariables;

    // gSettings.LeanCVar (§8.31): 0 the original lookup, which builds a std::string key and copies a
    // shared_ptr per read; 1 a transparent find returning a raw pointer, the same answer without
    // either; 2 (benchmark builds) a per-thread memo on the name pointer, a probe only - a pointer
    // can later hold a different name. Read when it is set, and on Load. Default 1 since §8.32: the
    // picture change §8.31 blamed on it was the benchmark's camera override; exact at both fixtures,
    // -1.5% Market and -1.7% Hyrule Field render-thread CPU at 1000 MHz.
    int mLeanCVar = 1;
    CVar* Find(const char* name);
    enum class LeanGetterKind { Integer, Float, String, Color };
    bool LeanGetter(LeanGetterKind kind) const;
    void Changed(const char* name, bool structural);
};
} // namespace Ship
