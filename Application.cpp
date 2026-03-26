#include "Application.h"
#include "Logger.h"
#include "Command.h"
#include "imgui/imgui.h"
#include "classes/TicTacToe.h"
#include "classes/Checkers.h"
#include "classes/Othello.h"
#include "classes/Chess.h"

namespace ClassGame {

    Game *game = nullptr;
    bool gameOver = false;
    int gameWinner = -1;

    // "Game Control" Window defaults
    static int gameActCounter = 0;                              // Track player actions by count
    static float floatVal = 0.0f;                               // Displays designated float val on slider
    static ImVec4 clearColor = ImVec4(0.5f, 0.5f, 0.5f, 1.00f); // Hex picker default

    static bool LogWin = true;
    static bool GameWin = true;      // Game window
    static bool ControlWin = true;   // Game control panel

    // Default hex color (clear)
    static float colorR = 115.0f / 255.0f;
    static float colorG = 140.0f / 255.0f;
    static float colorB = 153.0f / 255.0f;

    static char InputBuf[256] = "";
    
    // Game mode selection
    enum GameMode {
        MODE_HUMAN_VS_HUMAN = 0,
        MODE_HUMAN_VS_AI = 1,
        MODE_AI_VS_AI = 2
    };
    
    static int selectedGameMode = MODE_HUMAN_VS_HUMAN;
    static int aiPlayerNumber = 2;  // Which player is AI (1 or 2)
    static bool aiAsPlayer1 = false;
    
    // Chess-specific AI settings
    static int chessAIDepth = 3;  // Default search depth for chess AI
    static bool showAIAnalysis = true;  // Toggle for showing AI analysis

    //
    // Helper function to reset the current game
    //
    void ResetCurrentGame() 
    {
        if (game) {
            game->stopGame();
            game->setUpBoard();
            gameOver = false;
            gameWinner = -1;
            gameActCounter = 0;
            
            // Reconfigure AI settings
            if (game) {
                game->_gameOptions.AIvsAI = (selectedGameMode == MODE_AI_VS_AI);
                
                // Special handling for Chess to set AI depth
                Chess* chessGame = dynamic_cast<Chess*>(game);
                if (chessGame) {
                    chessGame->setSearchDepth(chessAIDepth);
                    if (selectedGameMode == MODE_HUMAN_VS_AI) {
                        chessGame->setAIPlayer(aiPlayerNumber - 1);
                        game->getPlayerAt(aiPlayerNumber - 1)->setAIPlayer(true);
                        game->getPlayerAt((aiPlayerNumber % 2))->setAIPlayer(false);
                        LOG_INFO_TAG("Chess AI set as Player " + std::to_string(aiPlayerNumber) + 
                                     " with depth " + std::to_string(chessAIDepth), "CHESS");
                    } else if (selectedGameMode == MODE_HUMAN_VS_HUMAN) {
                        chessGame->setAIPlayer(-1);
                        game->getPlayerAt(0)->setAIPlayer(false);
                        game->getPlayerAt(1)->setAIPlayer(false);
                    } else if (selectedGameMode == MODE_AI_VS_AI) {
                        // For AI vs AI, AI plays as white (player 0)
                        chessGame->setAIPlayer(0);
                        game->getPlayerAt(0)->setAIPlayer(true);
                        game->getPlayerAt(1)->setAIPlayer(true);
                        LOG_INFO_TAG("Chess AI vs AI mode with depth " + std::to_string(chessAIDepth), "CHESS");
                    }
                } else {
                    // Original logic for other games
                    if (selectedGameMode == MODE_HUMAN_VS_AI) {
                        game->getPlayerAt(aiPlayerNumber - 1)->setAIPlayer(true);
                        game->getPlayerAt((aiPlayerNumber % 2))->setAIPlayer(false);
                    } else if (selectedGameMode == MODE_HUMAN_VS_HUMAN) {
                        game->getPlayerAt(0)->setAIPlayer(false);
                        game->getPlayerAt(1)->setAIPlayer(false);
                    }
                }
            }
            
            LOG_INFO_TAG("Game reset - new game started", "GAME");
        }
    }

    //
    // game starting point
    // this is called by the main render loop in main.cpp
    //
    void GameStartUp() 
    {
        // Initialize Logger
        Logger::GetInstance().Init();
        
        // Allow user to choose game mode and start a new game
        game = nullptr;
        gameOver = false;
        gameWinner = -1;
        gameActCounter = 0;
        selectedGameMode = MODE_HUMAN_VS_HUMAN;
        aiPlayerNumber = 2;
        aiAsPlayer1 = false;
        chessAIDepth = 3;
        showAIAnalysis = true;
        
        LOG_INFO("Game application started. Select a game to begin.");
    }

    //
    // Start a new game with specified mode
    //
    void StartGameWithMode(Game* newGame, const std::string& gameName) 
    {
        delete game;
        game = newGame;
        game->setUpBoard();
        gameOver = false;
        gameWinner = -1;
        gameActCounter = 0;
        
        // Configure AI settings based on selected mode
        if (selectedGameMode == MODE_AI_VS_AI) {
            game->_gameOptions.AIvsAI = true;
            
            // Special handling for Chess
            Chess* chessGame = dynamic_cast<Chess*>(game);
            if (chessGame) {
                chessGame->setSearchDepth(chessAIDepth);
                chessGame->setAIPlayer(0); // AI plays as white in AI vs AI
                game->getPlayerAt(0)->setAIPlayer(true);
                game->getPlayerAt(1)->setAIPlayer(true);
                LOG_INFO_TAG(gameName + " started: AI vs AI (depth " + std::to_string(chessAIDepth) + ")", "CHESS");
            } else {
                game->getPlayerAt(0)->setAIPlayer(true);
                game->getPlayerAt(1)->setAIPlayer(true);
                LOG_INFO_TAG(gameName + " started: AI vs AI", "GAME");
            }
        } 
        else if (selectedGameMode == MODE_HUMAN_VS_AI) {
            game->_gameOptions.AIvsAI = false;
            
            // Special handling for Chess
            Chess* chessGame = dynamic_cast<Chess*>(game);
            if (chessGame) {
                chessGame->setSearchDepth(chessAIDepth);
                chessGame->setAIPlayer(aiPlayerNumber - 1);
                game->getPlayerAt(aiPlayerNumber - 1)->setAIPlayer(true);
                game->getPlayerAt((aiPlayerNumber % 2))->setAIPlayer(false);
                LOG_INFO_TAG(gameName + " started: Human vs AI (depth " + std::to_string(chessAIDepth) + ")", "CHESS");
            } else {
                game->getPlayerAt(aiPlayerNumber - 1)->setAIPlayer(true);
                game->getPlayerAt((aiPlayerNumber % 2))->setAIPlayer(false);
            }
            
            std::string humanPlayer = (aiPlayerNumber == 1) ? "Player 2" : "Player 1";
            std::string aiPlayer = (aiPlayerNumber == 1) ? "Player 1 (AI)" : "Player 2 (AI)";
            LOG_INFO_TAG(gameName + " started: " + humanPlayer + " vs " + aiPlayer, "GAME");
        }
        else {
            game->_gameOptions.AIvsAI = false;
            
            // Special handling for Chess
            Chess* chessGame = dynamic_cast<Chess*>(game);
            if (chessGame) {
                chessGame->setSearchDepth(chessAIDepth);
                chessGame->setAIPlayer(-1);
            }
            
            game->getPlayerAt(0)->setAIPlayer(false);
            game->getPlayerAt(1)->setAIPlayer(false);
            LOG_INFO_TAG(gameName + " started: Human vs Human", "GAME");
        }
        
        // Game-specific setup messages
        if (dynamic_cast<Chess*>(game)) {
            LOG_INFO_TAG("Player 1: White | Player 2: Black", "CHESS");
        } else if (dynamic_cast<TicTacToe*>(game)) {
            LOG_INFO_TAG("Player 1: X | Player 2: O", "GAME");
        }
    }

    //
    // game render loop
    // this is called by the main render loop in main.cpp
    //
    void RenderGame() 
    {
        ImGui::DockSpaceOverViewport();

        // Settings/Game Selection Window
        ImGui::Begin("Game Settings");

        if (gameOver && game) {
            ImGui::Text("Game Over!");
            if (gameWinner == -1) {
                ImGui::Text("It's a Draw!");
            } else {
                ImGui::Text("Winner: Player %d", gameWinner);
            }
            if (ImGui::Button("Play Again")) {
                ResetCurrentGame();
            }
        }
        
        // Game Mode Selection
        ImGui::Separator();
        ImGui::Text("Game Mode:");
        
        if (ImGui::RadioButton("Human vs Human", &selectedGameMode, MODE_HUMAN_VS_HUMAN)) {
            LOG_INFO_TAG("Game mode set to: Human vs Human", "SETTINGS");
        }
        
        if (ImGui::RadioButton("Human vs AI", &selectedGameMode, MODE_HUMAN_VS_AI)) {
            LOG_INFO_TAG("Game mode set to: Human vs AI", "SETTINGS");
        }
        
        // AI player selection for Human vs AI mode
        if (selectedGameMode == MODE_HUMAN_VS_AI) {
            ImGui::Indent();
            if (ImGui::RadioButton("AI as Player 1 (White)", &aiPlayerNumber, 1)) {
                LOG_INFO_TAG("AI set as Player 1 (White)", "SETTINGS");
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("AI as Player 2 (Black)", &aiPlayerNumber, 2)) {
                LOG_INFO_TAG("AI set as Player 2 (Black)", "SETTINGS");
            }
            ImGui::Unindent();
        }
        
        if (ImGui::RadioButton("AI vs AI", &selectedGameMode, MODE_AI_VS_AI)) {
            LOG_INFO_TAG("Game mode set to: AI vs AI", "SETTINGS");
        }
        
        ImGui::Separator();
        
        // Chess-specific AI settings (only show if Chess is selected or no game selected)
        ImGui::Text("Chess AI Settings:");
        ImGui::SliderInt("Search Depth", &chessAIDepth, 1, 5, "Depth %d");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Higher depth = stronger but slower AI\nDepth 3 recommended for good play");
        }
        
        ImGui::Checkbox("Show AI Analysis", &showAIAnalysis);
        
        ImGui::Separator();
        ImGui::Text("Select Game:");
        
        if (!game) {
            // Game selection buttons
            if (ImGui::Button("Start Tic-Tac-Toe")) {
                StartGameWithMode(new TicTacToe(), "Tic-Tac-Toe");
            }
            
            if (ImGui::Button("Start Checkers")) {
                StartGameWithMode(new Checkers(), "Checkers");
            }
            
            if (ImGui::Button("Start Othello")) {
                StartGameWithMode(new Othello(), "Othello");
            }
            
            if (ImGui::Button("Start Chess")) {
                StartGameWithMode(new Chess(), "Chess");
            }
        } else {
            // Display current game information
            std::string gameType = 
                dynamic_cast<TicTacToe*>(game) ? "Tic-Tac-Toe" :
                dynamic_cast<Chess*>(game) ? "Chess" :
                dynamic_cast<Checkers*>(game) ? "Checkers" :
                dynamic_cast<Othello*>(game) ? "Othello" : "Unknown";
            
            ImGui::Text("Current Game: %s", gameType.c_str());
            
            // Display game mode
            std::string modeStr;
            if (selectedGameMode == MODE_HUMAN_VS_HUMAN) {
                modeStr = "Human vs Human";
            } else if (selectedGameMode == MODE_HUMAN_VS_AI) {
                modeStr = "Human vs AI (AI: Player " + std::to_string(aiPlayerNumber) + ")";
                if (dynamic_cast<Chess*>(game)) {
                    modeStr += " [Depth: " + std::to_string(chessAIDepth) + "]";
                }
            } else {
                modeStr = "AI vs AI";
                if (dynamic_cast<Chess*>(game)) {
                    modeStr += " [Depth: " + std::to_string(chessAIDepth) + "]";
                }
            }
            ImGui::Text("Mode: %s", modeStr.c_str());
            
            // Player information
            if (dynamic_cast<Chess*>(game)) {
                ImGui::Text("Player 1: White");
                ImGui::Text("Player 2: Black");
            } else if (dynamic_cast<TicTacToe*>(game)) {
                ImGui::Text("Player 1: X");
                ImGui::Text("Player 2: O");
            }
            
            ImGui::Text("Current Player: %s", 
                game->getCurrentPlayer() ? 
                ("Player " + std::to_string(game->getCurrentPlayer()->playerNumber() + 1)).c_str() : 
                "None");
            
            if (game->getCurrentPlayer() && game->getCurrentPlayer()->isAIPlayer()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), " (AI)");
            }
            
            // Display board state string
            std::string state = game->stateString();
            ImGui::Text("Board State:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy##State")) {
                ImGui::SetClipboardText(state.c_str());
                LOG_INFO_TAG("Board state copied to clipboard", "GAME");
            }
            
            if (state.length() > 100) {
                // Truncate long state strings
                ImGui::TextWrapped("%s...", state.substr(0, 100).c_str());
            } else {
                ImGui::TextWrapped("%s", state.c_str());
            }
            
            ImGui::Separator();
            
            if (ImGui::Button("Reset Game")) {
                ResetCurrentGame();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Change Game")) {
                delete game;
                game = nullptr;
                gameOver = false;
                gameWinner = -1;
                gameActCounter = 0;
                LOG_INFO_TAG("Game cleared - select a new game", "GAME");
            }
            
            ImGui::SameLine();
            
            // Force AI move button (for testing)
            if ((selectedGameMode == MODE_HUMAN_VS_AI || selectedGameMode == MODE_AI_VS_AI) && 
                game->getCurrentPlayer() && game->getCurrentPlayer()->isAIPlayer()) {
                if (ImGui::Button("Force AI Move")) {
                    if (game->gameHasAI()) {
                        game->updateAI();
                        LOG_INFO_TAG("AI move forced manually", "DEBUG");
                    }
                }
            }
        }
        ImGui::End();

        // Game Window
        ImGui::Begin("Game Window", &GameWin, ImGuiWindowFlags_NoScrollbar);
        if (game) {
            // Handle AI moves if it's an AI turn (only if game is not over)
            if (!gameOver && game->gameHasAI() && game->getCurrentPlayer() && game->getCurrentPlayer()->isAIPlayer()) {
                game->updateAI();
            }
            
            // Draw the game board
            game->drawFrame();
            
            // Game-specific additional info for Chess
            Chess* chessGame = dynamic_cast<Chess*>(game);
            if (chessGame && !gameOver) {
                chessGame->setSearchDepth(chessAIDepth);
                ImGui::Separator();
                
                if (chessGame->getCurrentPlayer()) {
                    std::string playerColor = (game->getCurrentPlayer()->playerNumber() == 0) ? "White" : "Black";
                    ImGui::Text("%s's turn", playerColor.c_str());
                    
                    // Show AI analysis if enabled and it's AI's turn
                    if (showAIAnalysis && game->getCurrentPlayer()->isAIPlayer()) {
                        ImGui::TextColored(ImVec4(0, 1, 0, 1), "AI thinking with depth %d...", chessAIDepth);
                    }
                }
            }
        } else {
            ImGui::Text("No game selected. Choose a game from the Settings window.");
            ImGui::Text("Select a game mode and click a game button to start.");
        }
        ImGui::End();

        // Game Log Window
        if (LogWin) {
            ImGui::Begin("Game Log", &LogWin);

            // Filter state variables
            static bool showInfo = true;
            static bool showWarning = true;
            static bool showError = true;
            static bool showScores = false;
            static bool showDebug = false;

            // Options button and popup
            if (ImGui::Button("Options")) {
                ImGui::OpenPopup("OptionsPopup");
            }

            if (ImGui::BeginPopup("OptionsPopup")) {
                ImGui::Text("Filter Options");
                ImGui::Separator();
                
                ImGui::Checkbox("Show Info", &showInfo);
                ImGui::Checkbox("Show Warnings", &showWarning);
                ImGui::Checkbox("Show Errors", &showError);
                ImGui::Checkbox("Show AI Scores", &showScores);
                ImGui::Checkbox("Show Debug", &showDebug);
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Clear Log")) {
                    Logger::GetInstance().Clear();
                }
                
                ImGui::EndPopup();
            }

            // Test buttons
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                Logger::GetInstance().Clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Test Info")) {
                LOG_INFO("This is a test info message");
            }
            ImGui::SameLine();
            if (ImGui::Button("Test Warning")) {
                LOG_WARN("This is a test warning message");
            }
            ImGui::SameLine();
            if (ImGui::Button("Test Error")) {
                LOG_ERROR("This is a test error message");
            }
            ImGui::Separator();

            // Display log entries with filtering
            const auto& entries = Logger::GetInstance().GetEntries();
            const auto& colors = Logger::GetInstance().GetColors();
            
            const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
            ImGui::BeginChild("LogScrollRegion", ImVec2(0, -footer_height), true);
            
            for (size_t i = 0; i < entries.size(); i++) {
                bool display = true;
                
                if (entries[i].find("[INFO]") != std::string::npos && !showInfo) { 
                    display = false; 
                }
                else if (entries[i].find("[WARN]") != std::string::npos && !showWarning) { 
                    display = false; 
                }
                else if (entries[i].find("[ERROR]") != std::string::npos && !showError) { 
                    display = false; 
                }
                else if (entries[i].find("[AI SCORE]") != std::string::npos && !showScores) { 
                    display = false; 
                }
                else if (entries[i].find("[DEBUG]") != std::string::npos && !showDebug) { 
                    display = false; 
                }
                
                if (display) {
                    ImGui::PushStyleColor(ImGuiCol_Text, colors[i]);
                    ImGui::Text("%s", entries[i].c_str());
                    ImGui::PopStyleColor();
                }
            }
            
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();

            // Command line input
            ImGui::Separator();
            bool reclaim_focus = false;

            ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | 
                                                ImGuiInputTextFlags_EscapeClearsAll | 
                                                ImGuiInputTextFlags_CallbackHistory;
            
            if (ImGui::InputText("##CommandInput", InputBuf, IM_ARRAYSIZE(InputBuf), 
                                input_text_flags, &Command::TextEditCallbackStub)) {
                char* s = InputBuf;
                Command::Strtrim(s);
                if (s[0])
                    Command::ExecCommand(s);
                strcpy_s(s, IM_ARRAYSIZE(InputBuf), "");
                reclaim_focus = true;
            }
            
            ImGui::SetItemDefaultFocus();
            if (reclaim_focus)
                ImGui::SetKeyboardFocusHere(-1);

            ImGui::SameLine();
            ImGui::Text("Command");

            ImGui::SameLine();
            if (ImGui::Button("Help")) {
                LOG_INFO_TAG("Available commands: CLEAR, HELP, INFO, WARN, ERROR, RESET", "CMD");
            }

            ImGui::End();
        }

        // Game Control Panel
        if (ControlWin) {
            ImGui::Begin("Game Control", &ControlWin);
            ImGui::Text("Main Game Control Panel");

            // Window toggles
            ImGui::Checkbox("##LogCheck", &LogWin);
            ImGui::SameLine();
            ImGui::Text("Log Window");

            ImGui::Checkbox("##GameCheck", &GameWin);
            ImGui::SameLine();
            ImGui::Text("Game Window");

            ImGui::Separator();

            // Game control buttons
            ImGui::Text("Game Controls:");
            
            if (ImGui::Button("Reset Game")) {
                if (game) {
                    ResetCurrentGame();
                }
            }

            ImGui::SameLine();
            
            if (ImGui::Button("New Game")) {
                delete game;
                game = nullptr;
                gameOver = false;
                gameWinner = -1;
                gameActCounter = 0;
                LOG_INFO_TAG("Game cleared - ready for new game selection", "GAME");
            }

            // Game status display
            ImGui::Separator();
            ImGui::Text("Game Status:");
            if (game) {
                std::string gameType = 
                    dynamic_cast<TicTacToe*>(game) ? "Tic-Tac-Toe" :
                    dynamic_cast<Chess*>(game) ? "Chess" :
                    dynamic_cast<Checkers*>(game) ? "Checkers" :
                    dynamic_cast<Othello*>(game) ? "Othello" : "Unknown";
                    
                ImGui::Text("Game: %s", gameType.c_str());
                
                std::string modeStr;
                if (selectedGameMode == MODE_HUMAN_VS_HUMAN) {
                    modeStr = "Human vs Human";
                } else if (selectedGameMode == MODE_HUMAN_VS_AI) {
                    modeStr = "Human vs AI";
                } else {
                    modeStr = "AI vs AI";
                }
                ImGui::Text("Mode: %s", modeStr.c_str());
                
                if (game->getCurrentPlayer()) {
                    std::string playerText = "Current: Player " + 
                        std::to_string(game->getCurrentPlayer()->playerNumber() + 1);
                    if (game->getCurrentPlayer()->isAIPlayer()) {
                        playerText += " (AI)";
                    }
                    ImGui::Text("%s", playerText.c_str());
                }
                
                ImGui::Text("Game Over: %s", gameOver ? "Yes" : "No");
                if (gameOver) {
                    ImGui::Text("Winner: %s", 
                        gameWinner == -1 ? "Draw" : 
                        ("Player " + std::to_string(gameWinner)));
                }
                
                // Show Chess-specific info
                if (dynamic_cast<Chess*>(game)) {
                    ImGui::Text("Chess AI Depth: %d", chessAIDepth);
                }
            } else {
                ImGui::Text("No active game");
            }

            ImGui::Separator();
            
            // Visual controls
            ImGui::SliderFloat("##float", &floatVal, 0.0f, 1.0f, "%.3f");
            ImGui::SameLine();
            ImGui::Text("Test Slider");

            // Color edit control
            float colorArray[3] = { colorR, colorG, colorB };
            if (ImGui::ColorEdit3("Background Color", colorArray)) {
                colorR = colorArray[0];
                colorG = colorArray[1];
                colorB = colorArray[2];
                clearColor = ImVec4(colorR, colorG, colorB, 1.0f);
            }

            // Action counter
            if (ImGui::Button("Game Action")) {
                gameActCounter++;
                std::string msg = "Game action performed, counter: " + std::to_string(gameActCounter);
                LOG_INFO_TAG(msg, "GAME");
            }
            ImGui::SameLine();
            ImGui::Text("Action Counter: %d", gameActCounter);

            ImGui::Separator();
            ImGui::Text("Logging Test Buttons:");

            if (ImGui::Button("Log Game Event")) {
                LOG_INFO_TAG("Manual game event from control panel", "GAME");
            }
            ImGui::SameLine();
            if (ImGui::Button("Log Warnings")) {
                LOG_WARN("Manual warning from control panel");
            }
            ImGui::SameLine();
            if (ImGui::Button("Log Error")) {
                LOG_ERROR("Manual error from control panel");
            }

            ImGui::End();
        }
    }

    //
    // end turn is called by the game code at the end of each turn
    // this is where we check for a winner
    //
    void EndOfTurn() {
        if (!game) return;
        
        // Increment action counter
        gameActCounter++;
        
        // Check for winner or draw
        Player *winner = game->checkForWinner();
        if (winner) {
            gameWinner = winner->playerNumber() + 1;
            LOG_INFO_TAG("Game Over! Winner: Player " + std::to_string(gameWinner), "GAME");
            LOG_INFO_TAG("Final State: " + std::string(game->stateString()), "GAME");
            game->stopGame();
            gameOver = true;
            return;
        } 
        
        if (game->checkForDraw()) {
            gameWinner = -1;
            LOG_INFO_TAG("Game Over! It's a draw.", "GAME");
            game->stopGame();
            gameOver = true;
            return;
        }
        
        // Log the state string after the move (only if game didn't end)
        // Note: getCurrentPlayer() has already switched, so we need the previous player
        int previousPlayerNum = (game->getCurrentPlayer()->playerNumber() + 1) % 2 + 1;
        std::string state = game->stateString();
        
        // Log AI move information if it was AI's turn
        bool wasAITurn = game->getPlayerAt(previousPlayerNum - 1)->isAIPlayer();
        
        if (wasAITurn) {
            Chess* chessGame = dynamic_cast<Chess*>(game);
            if (chessGame) {
                LOG_INFO_TAG("AI (Player " + std::to_string(previousPlayerNum) + 
                            ") made a move (depth " + std::to_string(chessAIDepth) + ")", "CHESS");
            } else {
                LOG_INFO_TAG("AI (Player " + std::to_string(previousPlayerNum) + 
                            ") made a move", "AI");
            }
        }
        
        LOG_INFO_TAG("End of turn #" + std::to_string(gameActCounter) + 
                    " | Player: " + std::to_string(previousPlayerNum) +
                    (wasAITurn ? " (AI)" : "") +
                    " | Board State: " + state, 
                    "GAME");
    }
}