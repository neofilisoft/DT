#include "scripting/ScriptEngine.h"
#include "core/filesystem/FileSystem.h"
#include "core/logging/Logger.h"
namespace lacrima::script {
ScriptEngine::ScriptEngine(){OpenStandardLibraries();}
void ScriptEngine::OpenStandardLibraries(){m_lua.open_libraries(sol::lib::base,sol::lib::coroutine,sol::lib::string,sol::lib::table,sol::lib::math,sol::lib::utf8);}
bool ScriptEngine::LoadFile(const std::string& path){std::lock_guard<std::recursive_mutex> lock(m_luaMutex);auto contents=FileSystem::Get().ReadEntireFile(path);if(!contents.has_value()){LACRIMA_LOG_ERROR(LogCategory::Scripting,"ScriptEngine::LoadFile: could not read '{}'",path);return false;}std::string source(reinterpret_cast<const char*>(contents->data()),contents->size());return LoadString(source,path);}
bool ScriptEngine::LoadString(const std::string& source,const std::string& chunkName){std::lock_guard<std::recursive_mutex> lock(m_luaMutex);sol::protected_function_result result=m_lua.script(source,sol::script_pass_on_error,chunkName);if(!result.valid()){sol::error err=result;LACRIMA_LOG_ERROR(LogCategory::Scripting,"ScriptEngine::LoadString failed for chunk '{}': {}",chunkName,err.what());return false;}return true;}
bool ScriptEngine::HasGlobalFunction(const std::string& functionName)const{std::lock_guard<std::recursive_mutex> lock(m_luaMutex);sol::object obj=m_lua[functionName];return obj.valid()&&obj.get_type()==sol::type::function;}
void ScriptEngine::LogCallError(const std::string& functionName,const sol::protected_function_result& result){sol::error err=result;LACRIMA_LOG_ERROR(LogCategory::Scripting,"Lua call to '{}' failed: {}",functionName,err.what());}
}

