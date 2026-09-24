#include "ship/resource/BenchLookup.h"
#include "ship/config/ConsoleVariable.h"

#include <atomic>
#include <cstring>
#include <functional>
#include "ship/utils/filesystemtools/DiskFile.h"
#include "ship/utils/Utils.h"
#include "ship/config/Config.h"
#include "ship/Context.h"

#ifdef _MSC_VER
#define strdup _strdup
#endif

namespace Ship {

ConsoleVariable::ConsoleVariable() {
    Load();
}

ConsoleVariable::~ConsoleVariable() {
    SPDLOG_TRACE("destruct console variables");
}

namespace {
// Bumped by anything that can add, remove or reload an entry, so a memoised answer (null included)
// is never older than the table. Values change in place, behind the same pointer, so rewriting an
// existing variable leaves it alone - GuiWindow::SyncVisibilityConsoleVariable rewrites one every
// frame (§8.31).
std::atomic<uint32_t> gCVarGeneration{ 1 };
} // namespace

#ifdef SOH_BENCHMARK
// CVar writes, cumulative, whether or not they added an entry.
uint64_t gBenchCVarChanges = 0;
#endif

void ConsoleVariable::Changed(const char* name, bool structural) {
    if (structural) {
        gCVarGeneration.fetch_add(1, std::memory_order_relaxed);
    }
#ifdef SOH_BENCHMARK
    gBenchCVarChanges++;
#endif
    if (name == nullptr || strcmp(name, "gSettings.LeanCVar") == 0) {
        auto it = mVariables.find(std::string_view("gSettings.LeanCVar"));
        mLeanCVar = it != mVariables.end() && it->second->Type == ConsoleVariableType::Integer ? it->second->Integer : 0;
    }
}

CVar* ConsoleVariable::Find(const char* name) {
#ifdef SOH_BENCHMARK
    if (mLeanCVar == 2) {
        struct Memo {
            uint32_t generation = 0;
            std::unordered_map<const char*, CVar*> byName;
        };
        thread_local Memo memo;
        const uint32_t generation = gCVarGeneration.load(std::memory_order_relaxed);
        if (memo.generation != generation) {
            memo.byName.clear();
            memo.generation = generation;
        }
        auto hit = memo.byName.find(name);
        if (hit != memo.byName.end()) {
            return hit->second;
        }
        auto it = mVariables.find(std::string_view(name));
        CVar* variable = it != mVariables.end() ? it->second.get() : nullptr;
        memo.byName.emplace(name, variable);
        return variable;
    }
#endif
    auto it = mVariables.find(std::string_view(name));
    return it != mVariables.end() ? it->second.get() : nullptr;
}

std::shared_ptr<CVar> ConsoleVariable::Get(const char* name) {
    BENCH_CVAR_SCOPE("CV::Get");
    if (mLeanCVar != 0) {
        auto it = mVariables.find(std::string_view(name));
        return it != mVariables.end() ? it->second : nullptr;
    }
    auto it = mVariables.find(std::string(name));
    return it != mVariables.end() ? it->second : nullptr;
}

int32_t ConsoleVariable::GetInteger(const char* name, int32_t defaultValue) {
    if (mLeanCVar != 0) {
        const CVar* variable = Find(name);

        if (variable != nullptr && variable->Type == ConsoleVariableType::Integer) {
            return variable->Integer;
        }

        return defaultValue;
    }

    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Integer) {
        return variable->Integer;
    }

    return defaultValue;
}

float ConsoleVariable::GetFloat(const char* name, float defaultValue) {
    if (mLeanCVar != 0) {
        const CVar* variable = Find(name);

        if (variable != nullptr && variable->Type == ConsoleVariableType::Float) {
            return variable->Float;
        }

        return defaultValue;
    }

    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Float) {
        return variable->Float;
    }

    return defaultValue;
}

const char* ConsoleVariable::GetString(const char* name, const char* defaultValue) {
    if (mLeanCVar != 0) {
        const CVar* variable = Find(name);

        if (variable != nullptr && variable->Type == ConsoleVariableType::String) {
            return variable->String;
        }

        return defaultValue;
    }

    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::String) {
        return variable->String;
    }

    return defaultValue;
}

Color_RGBA8 ConsoleVariable::GetColor(const char* name, Color_RGBA8 defaultValue) {
    if (mLeanCVar != 0) {
        const CVar* variable = Find(name);

        if (variable != nullptr && variable->Type == ConsoleVariableType::Color) {
            return variable->Color;
        } else if (variable != nullptr && variable->Type == ConsoleVariableType::Color24) {
            Color_RGBA8 temp;
            temp.r = variable->Color24.r;
            temp.g = variable->Color24.g;
            temp.b = variable->Color24.b;
            temp.a = 255;
            return temp;
        }

        return defaultValue;
    }

    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Color) {
        return variable->Color;
    } else if (variable != nullptr && variable->Type == ConsoleVariableType::Color24) {
        Color_RGBA8 temp;
        temp.r = variable->Color24.r;
        temp.g = variable->Color24.g;
        temp.b = variable->Color24.b;
        temp.a = 255;
        return temp;
    }

    return defaultValue;
}

Color_RGB8 ConsoleVariable::GetColor24(const char* name, Color_RGB8 defaultValue) {
    if (mLeanCVar != 0) {
        const CVar* variable = Find(name);

        if (variable != nullptr && variable->Type == ConsoleVariableType::Color24) {
            return variable->Color24;
        } else if (variable != nullptr && variable->Type == ConsoleVariableType::Color) {
            Color_RGB8 temp;
            temp.r = variable->Color.r;
            temp.g = variable->Color.g;
            temp.b = variable->Color.b;
            return temp;
        }

        return defaultValue;
    }

    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Color24) {
        return variable->Color24;
    } else if (variable != nullptr && variable->Type == ConsoleVariableType::Color) {
        Color_RGB8 temp;
        temp.r = variable->Color.r;
        temp.g = variable->Color.g;
        temp.b = variable->Color.b;
        return temp;
    }

    return defaultValue;
}

void ConsoleVariable::SetInteger(const char* name, int32_t value) {
    BENCH_CVAR_WRITE_SCOPE("CV::SetInteger");
    auto& variable = mVariables[name];
    const bool added = variable == nullptr;
    if (added) {
        variable = std::make_shared<CVar>();
    }

    variable->Type = ConsoleVariableType::Integer;
    variable->Integer = value;
    Changed(name, added);
}

void ConsoleVariable::SetFloat(const char* name, float value) {
    BENCH_CVAR_WRITE_SCOPE("CV::SetFloat");
    auto& variable = mVariables[name];
    const bool added = variable == nullptr;
    if (added) {
        variable = std::make_shared<CVar>();
    }

    variable->Type = ConsoleVariableType::Float;
    variable->Float = value;
    Changed(name, added);
}

void ConsoleVariable::SetString(const char* name, const char* value) {
    BENCH_CVAR_WRITE_SCOPE("CV::SetString");
    auto& variable = mVariables[name];
    const bool added = variable == nullptr;
    if (added) {
        variable = std::make_shared<CVar>();
    }

    variable->Type = ConsoleVariableType::String;
    if (variable->String != nullptr) {
        free(variable->String);
    }
    variable->String = strdup(value);
    Changed(name, added);
}

void ConsoleVariable::SetColor(const char* name, Color_RGBA8 value) {
    BENCH_CVAR_WRITE_SCOPE("CV::SetColor");
    auto& variable = mVariables[name];
    const bool added = variable == nullptr;
    if (added) {
        variable = std::make_shared<CVar>();
    }

    variable->Type = ConsoleVariableType::Color;
    variable->Color = value;
    Changed(name, added);
}

void ConsoleVariable::SetColor24(const char* name, Color_RGB8 value) {
    BENCH_CVAR_WRITE_SCOPE("CV::SetColor24");
    auto& variable = mVariables[name];
    const bool added = variable == nullptr;
    if (added) {
        variable = std::make_shared<CVar>();
    }

    variable->Type = ConsoleVariableType::Color24;
    variable->Color24 = value;
    Changed(name, added);
}

void ConsoleVariable::RegisterInteger(const char* name, int32_t defaultValue) {
    if (Get(name) == nullptr) {
        SetInteger(name, defaultValue);
    }
}

void ConsoleVariable::RegisterFloat(const char* name, float defaultValue) {
    if (Get(name) == nullptr) {
        SetFloat(name, defaultValue);
    }
}

void ConsoleVariable::RegisterString(const char* name, const char* defaultValue) {
    if (Get(name) == nullptr) {
        SetString(name, defaultValue);
    }
}

void ConsoleVariable::RegisterColor(const char* name, Color_RGBA8 defaultValue) {
    if (Get(name) == nullptr) {
        SetColor(name, defaultValue);
    }
}

void ConsoleVariable::RegisterColor24(const char* name, Color_RGB8 defaultValue) {
    if (Get(name) == nullptr) {
        SetColor24(name, defaultValue);
    }
}

void ConsoleVariable::ClearVariable(const char* name) {
    BENCH_CVAR_WRITE_SCOPE("CV::ClearVariable");
    std::shared_ptr<Config> conf = Context::GetInstance()->GetConfig();
    auto var = Get(name);
    if (var != nullptr) {
        bool color = var->Type == ConsoleVariableType::Color || var->Type == ConsoleVariableType::Color24;
        if (color) {
            std::string a = StringHelper::Sprintf("%s.%s", name, "A");
            std::string b = StringHelper::Sprintf("%s.%s", name, "B");
            std::string g = StringHelper::Sprintf("%s.%s", name, "G");
            std::string r = StringHelper::Sprintf("%s.%s", name, "R");
            std::string t = StringHelper::Sprintf("%s.%s", name, "Type");
            mVariables.erase(a);
            mVariables.erase(b);
            mVariables.erase(g);
            mVariables.erase(r);
            mVariables.erase(t);
            conf->Erase(std::string("CVars.") + a);
            conf->Erase(std::string("CVars.") + b);
            conf->Erase(std::string("CVars.") + g);
            conf->Erase(std::string("CVars.") + r);
            conf->Erase(std::string("CVars.") + t);
        } else if (var->Type == ConsoleVariableType::String) {
            free(var->String);
            var->String = nullptr;
        }
    }
    mVariables.erase(name);
    conf->Erase(StringHelper::Sprintf("CVars.%s", name));
    Changed(name, true);
}

void ConsoleVariable::ClearBlock(const char* name) {
    std::shared_ptr<Config> conf = Context::GetInstance()->GetConfig();
    conf->EraseBlock(StringHelper::Sprintf("CVars.%s", name));
    Load();
}

void ConsoleVariable::CopyVariable(const char* from, const char* to) {
    BENCH_CVAR_WRITE_SCOPE("CV::CopyVariable");
    auto& variableFrom = mVariables[from];
    if (!variableFrom) {
        return;
    }
    auto& variableTo = mVariables[to];
    if (!variableTo) {
        variableTo = std::make_shared<CVar>();
    }

    variableTo->Type = variableFrom->Type;
    switch (variableTo->Type) {
        case ConsoleVariableType::Integer:
            variableTo->Integer = variableFrom->Integer;
            break;
        case ConsoleVariableType::Float:
            variableTo->Float = variableFrom->Float;
            break;
        case ConsoleVariableType::String:
            if (variableTo->String != nullptr) {
                free(variableTo->String);
            }
            variableTo->String = strdup(variableFrom->String);
            break;
        case ConsoleVariableType::Color:
            variableTo->Color = variableFrom->Color;
            break;
        case ConsoleVariableType::Color24:
            variableTo->Color24 = variableFrom->Color24;
            break;
    }
    Changed(to, true);
}

void ConsoleVariable::Save() {
    std::shared_ptr<Config> conf = Context::GetInstance()->GetConfig();

    for (const auto& variable : mVariables) {
        const std::string key = StringHelper::Sprintf("CVars.%s", variable.first.c_str());

        if (variable.second->Type == ConsoleVariableType::String && variable.second != nullptr) {
            conf->SetString(key, variable.second->String);
        } else if (variable.second->Type == ConsoleVariableType::Integer) {
            conf->SetInt(key, variable.second->Integer);
        } else if (variable.second->Type == ConsoleVariableType::Float) {
            conf->SetFloat(key, variable.second->Float);
        } else if (variable.second->Type == ConsoleVariableType::Color ||
                   variable.second->Type == ConsoleVariableType::Color24) {
            auto keyStr = key.c_str();
            conf->SetUInt(StringHelper::Sprintf("%s.R", keyStr), variable.second->Type == ConsoleVariableType::Color
                                                                     ? variable.second->Color.r
                                                                     : variable.second->Color24.r);
            conf->SetUInt(StringHelper::Sprintf("%s.G", keyStr), variable.second->Type == ConsoleVariableType::Color
                                                                     ? variable.second->Color.g
                                                                     : variable.second->Color24.g);
            conf->SetUInt(StringHelper::Sprintf("%s.B", keyStr), variable.second->Type == ConsoleVariableType::Color
                                                                     ? variable.second->Color.b
                                                                     : variable.second->Color24.b);
            if (variable.second->Type == ConsoleVariableType::Color) {
                conf->SetUInt(StringHelper::Sprintf("%s.A", keyStr), variable.second->Color.a);
                conf->SetString(StringHelper::Sprintf("%s.Type", keyStr), "RGBA");
            } else {
                conf->SetString(StringHelper::Sprintf("%s.Type", keyStr), "RGB");
            }
        }
    }

    conf->Save();
}

void ConsoleVariable::Load() {
    BENCH_CVAR_WRITE_SCOPE("CV::Load");
    std::shared_ptr<Config> conf = Context::GetInstance()->GetConfig();
    conf->Reload();
    if (!mVariables.empty()) {
        mVariables.clear();
    }

    LoadFromPath("", conf->GetNestedJson()["CVars"].items());

    LoadLegacy();
    Changed(nullptr, true);
}

void ConsoleVariable::LoadFromPath(
    std::string path, nlohmann::detail::iteration_proxy<nlohmann::detail::iter_impl<nlohmann::json>> items) {
    if (!path.empty()) {
        path += ".";
    }

    for (const auto& item : items) {
        std::string itemPath = path + item.key();
        auto value = item.value();
        switch (value.type()) {
            case nlohmann::detail::value_t::array:
                break;
            case nlohmann::detail::value_t::object:
                if (value.contains("Type") && value["Type"].get<std::string>() == "RGBA") {
                    Color_RGBA8 clr;
                    clr.r = value["R"].get<uint8_t>();
                    clr.g = value["G"].get<uint8_t>();
                    clr.b = value["B"].get<uint8_t>();
                    clr.a = value["A"].get<uint8_t>();
                    SetColor(itemPath.c_str(), clr);
                } else if (value.contains("Type") && value["Type"].get<std::string>() == "RGB") {
                    Color_RGB8 clr;
                    clr.r = value["R"].get<uint8_t>();
                    clr.g = value["G"].get<uint8_t>();
                    clr.b = value["B"].get<uint8_t>();
                    SetColor24(itemPath.c_str(), clr);
                } else {
                    LoadFromPath(itemPath, value.items());
                }

                break;
            case nlohmann::detail::value_t::string:
                SetString(itemPath.c_str(), value.get<std::string>().c_str());
                break;
            case nlohmann::detail::value_t::boolean:
                SetInteger(itemPath.c_str(), value.get<bool>());
                break;
            case nlohmann::detail::value_t::number_unsigned:
            case nlohmann::detail::value_t::number_integer:
                SetInteger(itemPath.c_str(), value.get<int>());
                break;
            case nlohmann::detail::value_t::number_float:
                SetFloat(itemPath.c_str(), value.get<float>());
                break;
            default:;
        }
    }
}
void ConsoleVariable::LoadLegacy() {
    auto conf = Context::GetPathRelativeToAppDirectory("cvars.cfg");
    if (DiskFile::Exists(conf)) {
        const auto lines = DiskFile::ReadAllLines(conf);

        for (const std::string& line : lines) {
            std::vector<std::string> cfg = StringHelper::Split(line, " = ");
            if (line.empty()) {
                continue;
            }
            if (cfg.size() < 2) {
                continue;
            }

            if (cfg[1].find("\"") == std::string::npos && (cfg[1].find("#") != std::string::npos)) {
                std::string value(cfg[1]);
                value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return c == '#'; }), value.end());
                auto splitTest = StringHelper::Split(value, "\r")[0];

                uint32_t val = std::stoul(splitTest, nullptr, 16);
                Color_RGBA8 clr;
                clr.r = val >> 24;
                clr.g = val >> 16;
                clr.b = val >> 8;
                clr.a = val & 0xFF;
                SetColor(cfg[0].c_str(), clr);
            }

            if (cfg[1].find("\"") != std::string::npos) {
                std::string value(cfg[1]);
                value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());
                SetString(cfg[0].c_str(), strdup(value.c_str()));
            }
            if (Math::IsNumber<float>(cfg[1])) {
                SetFloat(cfg[0].c_str(), std::stof(cfg[1]));
            }
            if (Math::IsNumber<int>(cfg[1])) {
                SetInteger(cfg[0].c_str(), std::stoi(cfg[1]));
            }
        }

        fs::remove(conf);
    }
}
} // namespace Ship
