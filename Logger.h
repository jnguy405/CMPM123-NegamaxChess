#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include "imgui/imgui.h"

namespace ClassGame {

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }
    
    // Initialize
    void Init(const std::string& filename = "game_log.txt");
    
    // Logging functions
    void Info(const std::string& message, const std::string& tag = "");
    void Warning(const std::string& message, const std::string& tag = "");
    void Error(const std::string& message, const std::string& tag = "");
    void GameEvent(const std::string& message);
    
    // UI display
    const std::vector<std::string>& GetEntries() const { return entries; }
    const std::vector<ImVec4>& GetColors() const { return colors; }
    void Clear();
    
private:
    Logger() = default;
    void AddEntry(const std::string& level, const std::string& message, 
                 const std::string& tag, const ImVec4& color);
    
    std::vector<std::string> entries;
    std::vector<ImVec4> colors;
    std::ofstream logFile;
    bool initialized = false;
};

// Macros
#define LOG_INFO(msg) ClassGame::Logger::GetInstance().Info(msg)
#define LOG_INFO_TAG(msg, tag) ClassGame::Logger::GetInstance().Info(msg, tag)
#define LOG_WARN(msg) ClassGame::Logger::GetInstance().Warning(msg)
#define LOG_WARN_TAG(msg, tag) ClassGame::Logger::GetInstance().Warning(msg, tag)
#define LOG_ERROR(msg) ClassGame::Logger::GetInstance().Error(msg)
#define LOG_ERROR_TAG(msg, tag) ClassGame::Logger::GetInstance().Error(msg, tag)
#define LOG_EVENT(msg) ClassGame::Logger::GetInstance().GameEvent(msg)

}