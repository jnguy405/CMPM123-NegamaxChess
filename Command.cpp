#include "Command.h"
#include "Logger.h"
#include "Application.h"
#include <string>
#include <cctype>
#include <cstring>
#include <cstdlib>

namespace ClassGame {
    namespace Command {
        
        // Static variables for command history
        static ImVector<char*> CommandHistory;
        static int HistoryPos = -1;
        
        // Case-insensitive string compare
        int Stricmp(const char* s1, const char* s2) { 
            int d; 
            while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { 
                s1++; 
                s2++; 
            } 
            return d; 
        }
        
        // Case-insensitive string compare with length limit
        int Strnicmp(const char* s1, const char* s2, int n) { 
            int d = 0; 
            while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { 
                s1++; 
                s2++; 
                n--; 
            } 
            return d; 
        }
        
        // Duplicate string
        char* Strdup(const char* s) { 
            size_t len = strlen(s) + 1; 
            void* buf = malloc(len); 
            return (char*)memcpy(buf, (const void*)s, len); 
        }
        
        // Trim trailing spaces
        void Strtrim(char* s) { 
            char* str_end = s + strlen(s); 
            while (str_end > s && str_end[-1] == ' ') 
                str_end--; 
            *str_end = 0; 
        }
        
        // Execute command from command line
        void ExecCommand(const char* command_line) {
            LOG_INFO_TAG(std::string("Command: ") + command_line, "CMD");
            
            // Add to history (remove duplicates)
            HistoryPos = -1;
            for (int i = CommandHistory.Size - 1; i >= 0; i--) {
                if (Stricmp(CommandHistory[i], command_line) == 0) {
                    free(CommandHistory[i]);
                    CommandHistory.erase(CommandHistory.begin() + i);
                    break;
                }
            }
            CommandHistory.push_back(Strdup(command_line));
            
            // Process commands
            if (Stricmp(command_line, "CLEAR") == 0) {
                Logger::GetInstance().Clear();
                LOG_INFO_TAG("Log cleared via command", "CMD");
            }
            else if (Stricmp(command_line, "HELP") == 0) {
                LOG_INFO_TAG("Available commands: CLEAR, HELP, INFO, WARN, ERROR, RESET", "CMD");
            }
            else if (Stricmp(command_line, "RESET") == 0) {
                // Access gameActCounter through a function in Application
                // For now, just log it
                LOG_INFO_TAG("Game counter reset", "CMD");
            }
            else if (Stricmp(command_line, "INFO") == 0) {
                LOG_INFO("Test info message from command line");
            }
            else if (Stricmp(command_line, "WARN") == 0) {
                LOG_WARN("Test warning message from command line");
            }
            else if (Stricmp(command_line, "ERROR") == 0) {
                LOG_ERROR("Test error message from command line");
            }
            else {
                LOG_ERROR_TAG(std::string("Unknown command: '") + command_line + "'", "CMD");
            }
        }
        
        // Callback for input text - handles command history navigation
        int TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
            switch (data->EventFlag) {
                case ImGuiInputTextFlags_CallbackHistory: {
                    const int prev_history_pos = HistoryPos;
                    if (data->EventKey == ImGuiKey_UpArrow) {
                        if (HistoryPos == -1)
                            HistoryPos = CommandHistory.Size - 1;
                        else if (HistoryPos > 0)
                            HistoryPos--;
                    }
                    else if (data->EventKey == ImGuiKey_DownArrow) {
                        if (HistoryPos != -1)
                            if (++HistoryPos >= CommandHistory.Size)
                                HistoryPos = -1;
                    }
                    
                    if (prev_history_pos != HistoryPos) {
                        const char* history_str = (HistoryPos >= 0) ? CommandHistory[HistoryPos] : "";
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, history_str);
                    }
                }
            }
            return 0;
        }
        
        // Clear command history
        void ClearHistory() {
            for (int i = 0; i < CommandHistory.Size; i++)
                free(CommandHistory[i]);
            CommandHistory.clear();
        }
        
        // Get command history
        const ImVector<char*>& GetHistory() {
            return CommandHistory;
        }
    }
}