#pragma once
#include "Game.h"
#include <vector>

// NOTE: This implementation assumes black.png and white.png exist in resources.
// If not, you can use o.png and x.png, or any other suitable graphics.

class Othello : public Game
{
public:
    Othello();
    ~Othello();

    // Required virtual methods from Game base class
    void        setUpBoard() override;
    Player*     checkForWinner() override;
    bool        checkForDraw() override;
    std::string initialStateString() override;
    std::string stateString() override;
    void        setStateString(const std::string &s) override;
    bool        actionForEmptyHolder(BitHolder &holder) override;
    bool        canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool        canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    void        stopGame() override;

    // AI methods
    void        updateAI() override;
    bool        gameHasAI() override { return true; } // Set to true when AI is implemented
    Grid* getGrid() override { return _grid; }

private:
    // Player constants
    static const int BLACK_PLAYER = 0;
    static const int WHITE_PLAYER = 1;

    // Direction vectors for checking all 8 directions
    static const int DIRECTIONS[8][2];

    // Helper methods
    Bit*        createPiece(Player* player);
    bool        isValidMove(int x, int y, Player* player) const;
    int         checkDirection(int x, int y, int dx, int dy, Player* player) const;
    void        flipPieces(int x, int y, Player* player);
    void        flipInDirection(int x, int y, int dx, int dy, Player* player, int count);
    bool        hasValidMove(Player* player) const;
    void        countPieces(int &blackCount, int &whiteCount) const;
    std::vector<std::pair<int, int>> getValidMoves(Player* player) const;
    void        showValidMoves(Player* player);
    void        clearValidMoveIndicators();

    // Board position helper
    void        getBoardPosition(BitHolder& holder, int &x, int &y) const;

    // Board representation
    Grid*       _grid;

    // Game state
    int         _consecutivePasses;
    bool        _showingHints;
};