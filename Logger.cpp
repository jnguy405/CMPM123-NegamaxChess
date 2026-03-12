#include "Logger.h"
#include <iomanip>
#include <sstream>

namespace ClassGame {

// Logger initialization and system feedback
void Logger::Init(const std::string& filename) {
    if (initialized) return;
    
    logFile.open(filename, std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        char timeStr[26];
        ctime_s(timeStr, sizeof(timeStr), &time_t);
    }
    
    initialized = true;
    Info("Game started successfully");
    Info("Application initialized", "GAME");
}

// Define entry pattern - timestamp, tag, and message
// Outputs to Game Log Window, console, and game_log.txt (in Debug folder or local)
void Logger::AddEntry(const std::string& level, const std::string& message, const std::string& tag, const ImVec4& color) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;
    
    std::stringstream ss;
    
    std::tm tm;
    localtime_s(&tm, &time_t);
    
    // Format: [HH:MM:SS.mmm]
    ss << "["
       << std::setfill('0') 
       << std::setw(2) << tm.tm_hour << ":"
       << std::setw(2) << tm.tm_min << ":"
       << std::setw(2) << tm.tm_sec
       << "." << std::setw(3) << ms.count()
       << "] ";
    
    // Add level: [INFO], [WARN], [ERROR]
    ss << "[" << level << "] ";
    
    // Add tag ([GAME])
    if (!tag.empty()) {
        ss << "[" << tag << "] ";
    }
    
    // Add the actual message
    ss << message;
    
    std::string entry = ss.str();
    entries.push_back(entry);
    colors.push_back(color);
    
    if (entries.size() > 1000) {
        entries.erase(entries.begin());
        colors.erase(colors.begin());
    }
    
    // Write to file
    if (logFile.is_open()) {
        logFile << entry << "\n";
        logFile.flush();
    }
    
    // Also print to console
    #ifdef _DEBUG
    printf("%s\n", entry.c_str());
    #endif
}

void Logger::Info(const std::string& message, const std::string& tag) {
    AddEntry("INFO", message, tag, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White
}

void Logger::Warning(const std::string& message, const std::string& tag) {
    AddEntry("WARN", message, tag, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
}

void Logger::Error(const std::string& message, const std::string& tag) {
    AddEntry("ERROR", message, tag, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red
}

// Specific way to assign or add tags and take priority of the first tag's color
void Logger::GameEvent(const std::string& message) {
    AddEntry("INFO", message, "GAME", ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White with [GAME] tag
}

void Logger::Clear() {
    entries.clear();
    colors.clear();
}

}