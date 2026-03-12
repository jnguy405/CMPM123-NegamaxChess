#pragma once

#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include "MagicBitboards.h"

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();
    void setUpBoard() override;
    void stopGame() override;

    // Player & turn management
    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;
    void makeMove(const BitMove& move);
    void moveCompleted(Bit* bit, BitHolder* src, BitHolder* dst);
    void undoMove(const BitMove& move, Bit* capturedPiece);
    Player* checkForWinner() override;
    bool checkForDraw() override;
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);

    // State string methods
    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }
    std::vector<BitMove> generateAllMoves();

    // AI methods
    bool gameHasAI() override;
    void updateAI() override;
    void setAIPlayer(int playerNumber);
    bool aiTestForTerminalState(std::string &state, Player *&winner);
    int negamax(std::string &state, int depth, int alpha, int beta, char currentColor);
    int evaluateBoard(const std::string &state);
    BitMove getBestMove() const { return _bestMove; }

private:
    Grid* _grid;
    BitMove _bestMove;

    // Piece bitboards
    uint64_t _whitePawns, _whiteKnights, _whiteBishops;
    uint64_t _whiteRooks, _whiteQueens,  _whiteKing;
    uint64_t _blackPawns, _blackKnights, _blackBishops;
    uint64_t _blackRooks, _blackQueens,  _blackKing;

    // Bitboard helpers
    uint64_t& getBitboard(ChessPiece pieceType, int playerNumber);
    uint64_t getWhitePieces() const;
    uint64_t getBlackPieces() const;
    uint64_t getAllPieces() const;
    void updateBitboardsFromGrid();
    void updateGridFromBitboards();
    void applyMoveToBitboards(const BitMove& move, int playerNumber);
    void stateStringToBitboards(const std::string& state);
    std::vector<BitMove> generateAllMovesFromState(const std::string& state, char currentColor);

    // Move generation
    void addPawnBitboardMovesToList(std::vector<BitMove>& moves, uint64_t bitboard, int shift);
    void generatePawnMoves(std::vector<BitMove>& moves, char color);
    void generatePieceMoves(std::vector<BitMove>& moves, char color, ChessPiece pieceType, uint64_t(*attackFn)(int, uint64_t));

    // Evaluation helpers
    int evaluateKingSafety(char color);
    int evaluatePawnStructure(char color);
    int getPieceValue(char pieceChar) const;
    int getPieceSquareValue(char pieceChar, int square) const;

    // Utility
    char pieceNotation(int x, int y) const;
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);

    int bitScanForward(uint64_t bb) const;
};