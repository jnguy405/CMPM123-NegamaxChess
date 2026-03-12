#pragma once
#include "imgui/imgui.h"

namespace ClassGame {
    namespace Command {
        // String helper functions
        int Stricmp(const char* s1, const char* s2);
        int Strnicmp(const char* s1, const char* s2, int n);
        char* Strdup(const char* s);
        void Strtrim(char* s);
        
        // Command execution
        void ExecCommand(const char* command_line);
        
        // Input callback for history navigation
        int TextEditCallbackStub(ImGuiInputTextCallbackData* data);
        
        // Command history management
        void ClearHistory();
        const ImVector<char*>& GetHistory();
    }
}