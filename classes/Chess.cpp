#include "Chess.h"
#include "MagicBitboards.h"
#include "Bitboard.h"
#include "evaluate.h"
#include <limits>
#include <cmath>
#include <cctype>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <climits>
#include <cstring>

// ============================================================================
// ALPHA-BETA PRUNING PARAMETERS
// ============================================================================

// Piece values (adjusted for better play which prioritize material and king safety more heavily)
const int PIECE_VALUES[6] = {
    100,   // Pawn
    320,   // Knight  
    330,   // Bishop
    500,   // Rook
    900,   // Queen
    20000  // King
};

// Bonus for controlling center squares (control of center helps with overall piece activity and mobility)
const int CENTER_BONUS[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0,
    0, 1, 2, 2, 2, 2, 1, 0,
    0, 1, 2, 3, 3, 2, 1, 0,
    0, 1, 2, 3, 3, 2, 1, 0,
    0, 1, 2, 2, 2, 2, 1, 0,
    0, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

// Penalty for doubled pawns (by file)
const int DOUBLED_PAWN_PENALTY = -10;

// Bonus for passed pawns (by rank)
const int PASSED_PAWN_BONUS[8] = {
    0, 10, 20, 30, 50, 80, 150, 0
};

// Bonus for bishop pair (helps with long-range control and piece coordination)
const int BISHOP_PAIR_BONUS = 30;

// Mobility bonus per move (encourages piece activity and control of the board)
const int MOBILITY_BONUS = 2;

// Move ordering bonuses (captures, killer moves, and history heuristic) prioritize bonuses then simple heuristics
const int ORDER_CAPTURE_BASE = 100000;
const int ORDER_KILLER_BASE = 80000;
const int ORDER_HISTORY_SCALE = 4;

// ============================================================================
// Special-move helpers (used by both applyMoveToBitboards and applyMoveToState)
// ============================================================================
static inline void getCastlingRookFromTo(char moverColor, bool kingSide, int& rookFrom, int& rookTo) {
    if (moverColor == 'w') {
        if (kingSide) { rookFrom = 7; rookTo = 5; }    // h1 -> f1
        else          { rookFrom = 0; rookTo = 3; }    // a1 -> d1
    } else {
        if (kingSide) { rookFrom = 63; rookTo = 61; }  // h8 -> f8
        else          { rookFrom = 56; rookTo = 59; }  // a8 -> d8
    }
}

static inline int enPassantCapturedSquare(char moverColor, const BitMove& move) {
    // Captured pawn is behind the destination square.
    return (moverColor == 'w') ? (static_cast<int>(move.to) - 8)
                               : (static_cast<int>(move.to) + 8);
}

static inline char promotedCharFromFlag(char moverColor, ChessPiece promotionPiece) {
    char base = (moverColor == 'w') ? 'Q' : 'q';
    switch (promotionPiece) {
        case Knight: return (moverColor == 'w') ? 'N' : 'n';
        case Bishop: return (moverColor == 'w') ? 'B' : 'b';
        case Rook:   return (moverColor == 'w') ? 'R' : 'r';
        case Queen:  return base;
        default:     return base;
    }
}

// ============================================================================
// Node snapshot helpers for save/restore code in search
// ============================================================================
Chess::NodeSearchState Chess::captureNodeSearchState() const {
    return NodeSearchState{
        _whitePawns, _whiteKnights, _whiteBishops, _whiteRooks, _whiteQueens, _whiteKing,
        _blackPawns, _blackKnights, _blackBishops, _blackRooks, _blackQueens, _blackKing,
        _whiteKingMoved, _blackKingMoved,
        _whiteRookAFileMoved, _whiteRookHFileMoved,
        _blackRookAFileMoved, _blackRookHFileMoved,
        _enPassantSquare
    };
}

void Chess::restoreNodeSearchState(const NodeSearchState& s) {
    _whitePawns = s.wp; _whiteKnights = s.wn; _whiteBishops = s.wb; _whiteRooks = s.wr; _whiteQueens = s.wq; _whiteKing = s.wk;
    _blackPawns = s.bp; _blackKnights = s.bn; _blackBishops = s.bb; _blackRooks = s.br; _blackQueens = s.bq; _blackKing = s.bk;
    _whiteKingMoved = s.wkm; _blackKingMoved = s.bkm;
    _whiteRookAFileMoved = s.wRA; _whiteRookHFileMoved = s.wRH;
    _blackRookAFileMoved = s.bRA; _blackRookHFileMoved = s.bRH;
    _enPassantSquare = s.ep;
}

// Move ordering - simple heuristic to try moves from the center outwards for better pruning
static const int MOVE_ORDER[64] = {
    27, 28, 29, 30, 31, 32, 33, 34,
    20, 21, 22, 23, 24, 25, 26, 35,
    19, 18, 17, 16, 15, 14, 13, 36,
    12, 11, 10, 9, 8, 7, 6, 37,
    5, 4, 3, 2, 1, 0, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63
};

// ============================================================================
// CONSTRUCTION & INITIALIZATION
// ============================================================================

// Attack function wrappers for consistent interface
static uint64_t knightAttacks(int sq, uint64_t) { return KnightAttacks[sq]; }
static uint64_t kingAttacks(int sq, uint64_t)   { return KingAttacks[sq]; }
static uint64_t bishopAttacks(int sq, uint64_t occupied) { return getBishopAttacks(sq, occupied); }
static uint64_t rookAttacks(int sq, uint64_t occupied) { return getRookAttacks(sq, occupied); }
static uint64_t queenAttacks(int sq, uint64_t occupied) { return getQueenAttacks(sq, occupied); }

static struct MagicInit {
    MagicInit() { initMagicBitboards(); }
    ~MagicInit() { cleanupMagicBitboards(); }
} magicInit;

Chess::Chess()
    : _whitePawns(0), _whiteKnights(0), _whiteBishops(0), _whiteRooks(0), _whiteQueens(0), _whiteKing(0),
      _blackPawns(0), _blackKnights(0), _blackBishops(0), _blackRooks(0), _blackQueens(0), _blackKing(0),
      _searchDepth(3), _whiteKingMoved(false), _blackKingMoved(false),
      _whiteRookAFileMoved(false), _whiteRookHFileMoved(false),
      _blackRookAFileMoved(false), _blackRookHFileMoved(false),
      _enPassantSquare(-1),
      _halfMoveClock(0)
{
    _grid = new Grid(8, 8);
    _bestMove = BitMove();
    std::memset(_killerFrom, -1, sizeof(_killerFrom));
    std::memset(_killerTo, -1, sizeof(_killerTo));
    std::memset(_historyHeuristic, 0, sizeof(_historyHeuristic));
}

Chess::~Chess()
{
    delete _grid;
}

// Set up the chess board with pieces in their initial positions and reset game state variables
void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;
    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
    _whiteKingMoved = _blackKingMoved = false;
    _whiteRookAFileMoved = _whiteRookHFileMoved = false;
    _blackRookAFileMoved = _blackRookHFileMoved = false;
    _enPassantSquare = -1;
    resetDrawRulesState();
    startGame();
}

// Clean up the board and reset all game state variables to prepare for a new game or exit
void Chess::stopGame() {
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
    
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
    _enPassantSquare = -1;
    resetDrawRulesState();
}

void Chess::resetDrawRulesState() {
    _halfMoveClock = 0;
    _repetitionCounts.clear();

    // Record the initial position at the start of a new game.
    if (_grid) {
        std::string key = repetitionKey();
        _repetitionCounts[key] = 1;
    }
}

void Chess::updateHalfMoveClock(const BitMove& move) {
    // 50-move rule resets on pawn moves and any captures.
    bool pawnMove = (move.piece == Pawn);
    bool captureMove = (move.flags & BitMove::MoveCapture) || (move.flags & BitMove::MoveEnPassant);

    if (pawnMove || captureMove) _halfMoveClock = 0;
    else _halfMoveClock++;
}

std::string Chess::repetitionKey() {
    std::string s = stateString();

    char sideToMove = (getCurrentPlayer() && getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';

    int wK = (!_whiteKingMoved && !_whiteRookHFileMoved) ? 1 : 0;
    int wQ = (!_whiteKingMoved && !_whiteRookAFileMoved) ? 1 : 0;
    int bK = (!_blackKingMoved && !_blackRookHFileMoved) ? 1 : 0;
    int bQ = (!_blackKingMoved && !_blackRookAFileMoved) ? 1 : 0;

    s.push_back('|');
    s.push_back(sideToMove);
    s.push_back(static_cast<char>('0' + wK));
    s.push_back(static_cast<char>('0' + wQ));
    s.push_back(static_cast<char>('0' + bK));
    s.push_back(static_cast<char>('0' + bQ));
    s.push_back('|');
    s += std::to_string(_enPassantSquare + 1); // -1 becomes 0
    return s;
}

// Set the search depth for the AI (with limits to prevent excessive computation time)
void Chess::setSearchDepth(int depth) {
    _searchDepth = std::max(1, std::min(depth, 8));
}


// Convert a FEN string to the internal board representation, setting up the pieces and bitboards accordingly
void Chess::FENtoBoard(const std::string& fen) {
    std::istringstream iss(fen);
    std::string boardPosition;
    iss >> boardPosition;
    
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            _grid->getSquare(x, y)->setBit(nullptr);
    
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
    
    int row = 7;
    int col = 0;
    
    for (char c : boardPosition) {
        if (c == '/') {
            row--;
            col = 0;
        } else if (std::isdigit(c)) {
            col += (c - '0');
        } else {
            int playerNumber = (std::isupper(c) ? 0 : 1);
            ChessPiece pieceType;
            int square = SQUARE(row, col);

            char upperC = std::toupper(c);
            switch (upperC) {
                case 'P': pieceType = Pawn;   break;
                case 'N': pieceType = Knight; break;
                case 'B': pieceType = Bishop; break;
                case 'R': pieceType = Rook;   break;
                case 'Q': pieceType = Queen;  break;
                case 'K': pieceType = King;   break;
                default:  pieceType = Pawn;   break;
            }
            SET_BIT(getBitboard(pieceType, playerNumber), square);
            
            Bit* piece = PieceForPlayer(playerNumber, pieceType);
            ChessSquare* squarePtr = _grid->getSquare(col, row);
            piece->setPosition(squarePtr->getPosition());
            squarePtr->setBit(piece);
            
            col++;
        }
    }
}

// ============================================================================
// PLAYER & TURN MANAGEMENT
// ============================================================================

// Check if a move can be made from the source square based on the piece's ownership and current player's turn
bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    int currentPlayerNum = getCurrentPlayer()->playerNumber();
    int pieceColor = (bit.gameTag() >= 128) ? 1 : 0;
    return (pieceColor == currentPlayerNum);
}

// Check if a move can be made from the source square to the destination square based on piece movement rules and current board state
bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);
    
    if (!srcSquare || !dstSquare) return false;
    
    int fromSquare = SQUARE(srcSquare->getRow(), srcSquare->getColumn());
    int toSquare = SQUARE(dstSquare->getRow(), dstSquare->getColumn());
    
    if (fromSquare == toSquare) return false;
    
    std::vector<BitMove> moves = generateAllMoves();
    
    for (const auto& move : moves)
        if (move.from == fromSquare && move.to == toSquare)
            return true;
    
    return false;
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

// Apply a move to the board, updating both the grid and the bitboards accordingly, and then end the turn   
void Chess::applyMoveToBitboards(const BitMove& move, int playerNumber) {
    ChessPiece pieceType = static_cast<ChessPiece>(move.piece);
    int enemyPlayer = (playerNumber == 0) ? 1 : 0;
    uint64_t toMask = (1ULL << move.to);
    bool isEnPassant = (move.flags & BitMove::MoveEnPassant) != 0;

    // Castling rights are lost if the king moves, or if the rook that started on
    // its original square moves/captured. Do NOT lose rights just because a piece
    // lands on a rook's starting square.
    bool capturedWhiteRookA1 = (!isEnPassant) && (move.to == 0)  && ((getBitboard(Rook, 0) & toMask) != 0);
    bool capturedWhiteRookH1 = (!isEnPassant) && (move.to == 7)  && ((getBitboard(Rook, 0) & toMask) != 0);
    bool capturedBlackRookA8 = (!isEnPassant) && (move.to == 56) && ((getBitboard(Rook, 1) & toMask) != 0);
    bool capturedBlackRookH8 = (!isEnPassant) && (move.to == 63) && ((getBitboard(Rook, 1) & toMask) != 0);

    // CAPTURES (normal and en passant)
    // Normal captures remove the piece on the destination square
    // En passant captures remove the pawn on the square behind the destination square
    if (move.flags & BitMove::MoveEnPassant) {
        char moverColor = (playerNumber == 0) ? 'w' : 'b';
        int capturedSquare = enPassantCapturedSquare(moverColor, move);
        if (capturedSquare >= 0 && capturedSquare < 64) {
            if (playerNumber == 0) CLEAR_BIT(_blackPawns, capturedSquare);
            else CLEAR_BIT(_whitePawns, capturedSquare);
        }
    } else {
        static const ChessPiece types[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
        for (int i = 0; i < 6; i++) {
            CLEAR_BIT(getBitboard(types[i], enemyPlayer), move.to);
        }
    }

    // PROMOTIONS (handled by setting the destination bit to the promoted piece type instead of the original piece type)
    // If it's a promotion move, we set the destination bit to the promoted piece type. Otherwise, we just move the original piece type.
    CLEAR_BIT(getBitboard(pieceType, playerNumber), move.from);
    if ((move.flags & BitMove::MovePromotion) && move.promotion != NoPiece) {
        SET_BIT(getBitboard(static_cast<ChessPiece>(move.promotion), playerNumber), move.to);
    } else {
        SET_BIT(getBitboard(pieceType, playerNumber), move.to);
    }

    // CASTLING (handled by moving the rook in addition to the king when a castling move is made)
    if (move.flags & BitMove::MoveCastleKingSide) {
        char moverColor = (playerNumber == 0) ? 'w' : 'b';
        int rookFrom, rookTo;
        getCastlingRookFromTo(moverColor, true, rookFrom, rookTo);
        CLEAR_BIT(getBitboard(Rook, playerNumber), rookFrom);
        SET_BIT(getBitboard(Rook, playerNumber), rookTo);

        // Mark rook as moved from its original square.
        if (rookFrom == 0) _whiteRookAFileMoved = true;
        else if (rookFrom == 7) _whiteRookHFileMoved = true;
        else if (rookFrom == 56) _blackRookAFileMoved = true;
        else if (rookFrom == 63) _blackRookHFileMoved = true;
    } else if (move.flags & BitMove::MoveCastleQueenSide) {
        char moverColor = (playerNumber == 0) ? 'w' : 'b';
        int rookFrom, rookTo;
        getCastlingRookFromTo(moverColor, false, rookFrom, rookTo);
        CLEAR_BIT(getBitboard(Rook, playerNumber), rookFrom);
        SET_BIT(getBitboard(Rook, playerNumber), rookTo);

        // Mark rook as moved from its original square.
        if (rookFrom == 0) _whiteRookAFileMoved = true;
        else if (rookFrom == 7) _whiteRookHFileMoved = true;
        else if (rookFrom == 56) _blackRookAFileMoved = true;
        else if (rookFrom == 63) _blackRookHFileMoved = true;
    }

    // Update castling rights and en-passant square
    if (pieceType == King) {
        if (playerNumber == 0) _whiteKingMoved = true;
        else _blackKingMoved = true;
    }
    if (pieceType == Rook) {
        if (playerNumber == 0) {
            if (move.from == 0) _whiteRookAFileMoved = true;
            if (move.from == 7) _whiteRookHFileMoved = true;
        } else {
            if (move.from == 56) _blackRookAFileMoved = true;
            if (move.from == 63) _blackRookHFileMoved = true;
        }
    }

    // If a rook on its starting square gets captured, castling is no longer
    // legal on that side.
    if (capturedWhiteRookA1) _whiteRookAFileMoved = true;
    if (capturedWhiteRookH1) _whiteRookHFileMoved = true;
    if (capturedBlackRookA8) _blackRookAFileMoved = true;
    if (capturedBlackRookH8) _blackRookHFileMoved = true;

    _enPassantSquare = -1;
    if (pieceType == Pawn && std::abs((int)move.to - (int)move.from) == 16) {
        _enPassantSquare = (move.from + move.to) / 2;
    }
}

// Apply a move to the board, update grid and bitboards, and then end the turn
void Chess::makeMove(const BitMove& move) {
    updateBitboardsFromGrid();
    
    int currentPlayer = getCurrentPlayer()->playerNumber();
    applyMoveToBitboards(move, currentPlayer);
    updateHalfMoveClock(move);
    
    updateGridFromBitboards();
    endTurn();
}

// Update the board state after a move is completed,grid and bitboards are synchronized, and then end the turn
void Chess::moveCompleted(Bit* bit, BitHolder* src, BitHolder* dst)
{
    if (!bit || !src || !dst) return;
    
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(dst);
    
    if (!srcSquare || !dstSquare) return;
    
    int fromSquare = SQUARE(srcSquare->getRow(), srcSquare->getColumn());
    int toSquare = SQUARE(dstSquare->getRow(), dstSquare->getColumn());
    
    int gameTag = bit->gameTag();
    ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
    int playerNumber = (gameTag < 128) ? 0 : 1;
    
    BitMove move(fromSquare, toSquare, pieceType);
    
    updateBitboardsFromGrid();
    applyMoveToBitboards(move, playerNumber);
    
    updateGridFromBitboards();
    endTurn();
}

void Chess::undoMove(const BitMove& move, Bit* capturedPiece) {
    // This would need implementation for full undo functionality
    endTurn();
}


// Check for a winner by verifying if either king has been captured (i.e., if the corresponding bitboard is empty)
// Return the winning player
Player* Chess::checkForWinner() {
    updateBitboardsFromGrid();
    if (_whiteKing == 0) return getPlayerAt(1);
    if (_blackKing == 0) return getPlayerAt(0);

    // Checkmate: side to move has no legal moves and its king is in check.
    char currentColor = (getCurrentPlayer() && getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';
    if (generateAllMoves().empty() && isKingInCheck(currentColor)) {
        // Winner is the opponent of the checkmated side.
        return getPlayerAt(currentColor == 'w' ? 1 : 0);
    }
    return nullptr;
}

// Check for a draw by verifying if the current player has any legal moves available
// returns true if not (indicating stalemate or checkmate)
bool Chess::checkForDraw() {
    // Update repetition counts for this position.
    std::string key = repetitionKey();
    int& count = _repetitionCounts[key];
    count++;

    // Threefold repetition draw.
    if (count >= 3) return true;

    // 50-move rule draw.
    if (_halfMoveClock >= 100) return true;

    // Stalemate draw (checkmate is handled in checkForWinner()).
    char currentColor = (getCurrentPlayer() && getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';
    return generateAllMoves().empty() && !isKingInCheck(currentColor);
}

// Grabs the piece sprite based on the player number and piece type
// returns a new Bit with the correct texture and game tag for ownership and type identification
Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };
    Bit* bit = new Bit();
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);
    bit->setGameTag(piece + (playerNumber == 0 ? 0 : 128));
    return bit;
}

// Get the owner of the piece at the specified coordinates
// Returns a pointer to the Player who owns the piece, or nullptr if the square is empty or out of bounds
Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return nullptr;
    auto square = _grid->getSquare(x, y);
    return (square && square->bit()) ? square->bit()->getOwner() : nullptr;
}

// ============================================================================
// STATE STRING METHODS
// ============================================================================

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            s += pieceNotation(x, y);
    return s;
}

void Chess::setStateString(const std::string &s)
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            _grid->getSquare(x, y)->setBit(nullptr);
    stateStringToBitboards(s);
    updateGridFromBitboards();
}

char Chess::pieceNotation(int x, int y) const {
    const char *wpieces = "0PNBRQK";
    const char *bpieces = "0pnbrqk";
    Bit *bit = _grid->getSquare(x, y)->bit();
    return bit ? (bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128]) : '0';
}

// ============================================================================
// BITBOARD HELPERS
// ============================================================================

// Take the current grid state and update all bitboards to match the pieces on the board
// Used for: synchronizing the internal bitboard representation with the visual board state after moves are made or undone
//           and for initializing the bitboards from a FEN string
void Chess::updateBitboardsFromGrid() {
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            Bit* piece = _grid->getSquare(col, row)->bit();
            if (!piece) continue;
            
            int square = SQUARE(row, col);
            int gameTag = piece->gameTag();
            ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
            
            SET_BIT(getBitboard(pieceType, gameTag < 128 ? 0 : 1), square);
        }
    }
}

// Take the current bitboard states and update the grid to match the pieces represented by the bitboards
// Used for: synchronizing the visual board state with the internal bitboard representation after moves are made or undone
void Chess::updateGridFromBitboards() {
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            _grid->getSquare(col, row)->setBit(nullptr);
    
    auto placePieces = [this](uint64_t bitboard, int playerNumber, ChessPiece pieceType) {
        if (bitboard == 0) return;
        BitboardElement bb(bitboard);
        bb.forEachBit([this, playerNumber, pieceType](int square) {
            int row = square / 8;
            int col = square % 8;
            Bit* piece = PieceForPlayer(playerNumber, pieceType);
            ChessSquare* squarePtr = _grid->getSquare(col, row);
            piece->setPosition(squarePtr->getPosition());
            squarePtr->setBit(piece);
        });
    };
    
    static const ChessPiece types[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
    for (int player = 0; player < 2; player++)
        for (int i = 0; i < 6; i++)
            placePieces(getBitboard(types[i], player), player, types[i]);
}

// Get a reference to the bitboard for the specified piece type and player number (0 for white, 1 for black)
// Used for: quick access and modification of the bitboards based on piece type and ownership
uint64_t& Chess::getBitboard(ChessPiece pieceType, int playerNumber) {
    if (playerNumber == 0) {
        switch (pieceType) {
            case Pawn:   return _whitePawns;
            case Knight: return _whiteKnights;
            case Bishop: return _whiteBishops;
            case Rook:   return _whiteRooks;
            case Queen:  return _whiteQueens;
            case King:   return _whiteKing;
            default:     return _whitePawns;
        }
    } else {
        switch (pieceType) {
            case Pawn:   return _blackPawns;
            case Knight: return _blackKnights;
            case Bishop: return _blackBishops;
            case Rook:   return _blackRooks;
            case Queen:  return _blackQueens;
            case King:   return _blackKing;
            default:     return _blackPawns;
        }
    }
}

uint64_t Chess::getWhitePieces() const {
    return _whitePawns | _whiteKnights | _whiteBishops | _whiteRooks | _whiteQueens | _whiteKing;
}

uint64_t Chess::getBlackPieces() const {
    return _blackPawns | _blackKnights | _blackBishops | _blackRooks | _blackQueens | _blackKing;
}

uint64_t Chess::getAllPieces() const {
    return getWhitePieces() | getBlackPieces();
}

// ============================================================================
// MOVE GENERATION
// ============================================================================

// Creates a list of all possible moves for the current player based on the current board state
void Chess::addPawnBitboardMovesToList(std::vector<BitMove>& moves, uint64_t bitboard, int shift, uint8_t baseFlags) {
    BitboardElement(bitboard).forEachBit([&](int toSquare) {
        int fromSquare = toSquare - shift;
        bool isPromotion = (toSquare / 8 == 0 || toSquare / 8 == 7);
        if (isPromotion) {
            moves.emplace_back(fromSquare, toSquare, Pawn, Queen,
                                static_cast<uint8_t>(BitMove::MovePromotion | baseFlags));
        } else {
            moves.emplace_back(fromSquare, toSquare, Pawn, NoPiece, baseFlags);
        }
    });
}


// Generate all possible moves for PAWNS, including single and double moves, captures, en-passant, and promotions
// Based on the current board state and the player's color
void Chess::generatePawnMoves(std::vector<BitMove>& moves, char color) {
    uint64_t pawns = (color == 'w') ? _whitePawns : _blackPawns;
    if (pawns == 0) return;

    uint64_t empty = ~getAllPieces();
    uint64_t enemy = (color == 'w') ? getBlackPieces() : getWhitePieces();

    constexpr uint64_t Rank3 = 0x0000000000FF0000ULL;
    constexpr uint64_t Rank6 = 0x0000FF0000000000ULL;

    uint64_t singleMoves = (color == 'w') ? NORTH(pawns) & empty : SOUTH(pawns) & empty;
    uint64_t doubleMoves = (color == 'w') ? NORTH(singleMoves & Rank3) & empty 
                                           : SOUTH(singleMoves & Rank6) & empty;
    uint64_t capturesLeft  = (color == 'w') ? NORTH_WEST(pawns) & enemy : SOUTH_WEST(pawns) & enemy;
    uint64_t capturesRight = (color == 'w') ? NORTH_EAST(pawns) & enemy : SOUTH_EAST(pawns) & enemy;

    int shiftForward      = (color == 'w') ?  8  : -8;
    int shiftDouble       = (color == 'w') ?  16 : -16;
    int shiftCaptureLeft  = (color == 'w') ?  7  : -9;
    int shiftCaptureRight = (color == 'w') ?  9  : -7;

    addPawnBitboardMovesToList(moves, singleMoves,   shiftForward, BitMove::MoveNone);
    addPawnBitboardMovesToList(moves, doubleMoves,   shiftDouble, BitMove::MoveNone);
    addPawnBitboardMovesToList(moves, capturesLeft,  shiftCaptureLeft, BitMove::MoveCapture);
    addPawnBitboardMovesToList(moves, capturesRight, shiftCaptureRight, BitMove::MoveCapture);

    // Add en-passant captures if the target square is currently available.
    if (_enPassantSquare >= 0 && _enPassantSquare < 64) {
        uint64_t epMask = (1ULL << _enPassantSquare);
        if (color == 'w') {
            uint64_t fromLeft = SOUTH_EAST(epMask) & pawns;
            uint64_t fromRight = SOUTH_WEST(epMask) & pawns;
            BitboardElement(fromLeft | fromRight).forEachBit([&](int fromSquare) {
                moves.emplace_back(fromSquare, _enPassantSquare, Pawn, NoPiece,
                                   BitMove::MoveEnPassant | BitMove::MoveCapture);
            });
        } else {
            uint64_t fromLeft = NORTH_EAST(epMask) & pawns;
            uint64_t fromRight = NORTH_WEST(epMask) & pawns;
            BitboardElement(fromLeft | fromRight).forEachBit([&](int fromSquare) {
                moves.emplace_back(fromSquare, _enPassantSquare, Pawn, NoPiece,
                                   BitMove::MoveEnPassant | BitMove::MoveCapture);
            });
        }
    }
}

// Generate moves for knights, bishops, rooks, and queens by using the appropriate attack functions 
// to find valid destination squares based on the current board state and piece positions
void Chess::generatePieceMoves(std::vector<BitMove>& moves, char color,
                                ChessPiece pieceType, uint64_t(*attackFn)(int, uint64_t)) {
    uint64_t pieces   = getBitboard(pieceType, color == 'w' ? 0 : 1);
    uint64_t friendly = (color == 'w') ? getWhitePieces() : getBlackPieces();
    uint64_t occupied = getAllPieces();

    if (pieces == 0) return;

    BitboardElement pieceBB(pieces);
    pieceBB.forEachBit([&](int fromSquare) {
        uint64_t attacks = attackFn(fromSquare, occupied) & ~friendly;
        BitboardElement attackBB(attacks);
        attackBB.forEachBit([&](int toSquare) {
            uint8_t flags = (GET_BIT((color == 'w') ? getBlackPieces() : getWhitePieces(), toSquare) != 0)
                                ? BitMove::MoveCapture
                                : BitMove::MoveNone;
            moves.emplace_back(fromSquare, toSquare, pieceType, NoPiece, flags);
        });
    });
}

// Generate castling moves for the king if the appropriate conditions are met (king and rook haven't moved, path is clear, and squares aren't attacked)
void Chess::generateCastlingMoves(std::vector<BitMove>& moves, char color) {
    // Verify castling rights and empty path squares.
    if (color == 'w') {
        if (!_whiteKingMoved && !isKingInCheck('w')) {
            bool canKingSide = !_whiteRookHFileMoved &&
                               !GET_BIT(getAllPieces(), 5) &&
                               !GET_BIT(getAllPieces(), 6) &&
                               !isSquareAttacked(5, 'b') &&
                               !isSquareAttacked(6, 'b');
            if (canKingSide) moves.emplace_back(4, 6, King, NoPiece, BitMove::MoveCastleKingSide);

            bool canQueenSide = !_whiteRookAFileMoved &&
                                !GET_BIT(getAllPieces(), 1) &&
                                !GET_BIT(getAllPieces(), 2) &&
                                !GET_BIT(getAllPieces(), 3) &&
                                !isSquareAttacked(3, 'b') &&
                                !isSquareAttacked(2, 'b');
            if (canQueenSide) moves.emplace_back(4, 2, King, NoPiece, BitMove::MoveCastleQueenSide);
        }
    } else {
        if (!_blackKingMoved && !isKingInCheck('b')) {
            bool canKingSide = !_blackRookHFileMoved &&
                               !GET_BIT(getAllPieces(), 61) &&
                               !GET_BIT(getAllPieces(), 62) &&
                               !isSquareAttacked(61, 'w') &&
                               !isSquareAttacked(62, 'w');
            if (canKingSide) moves.emplace_back(60, 62, King, NoPiece, BitMove::MoveCastleKingSide);

            bool canQueenSide = !_blackRookAFileMoved &&
                                !GET_BIT(getAllPieces(), 57) &&
                                !GET_BIT(getAllPieces(), 58) &&
                                !GET_BIT(getAllPieces(), 59) &&
                                !isSquareAttacked(59, 'w') &&
                                !isSquareAttacked(58, 'w');
            if (canQueenSide) moves.emplace_back(60, 58, King, NoPiece, BitMove::MoveCastleQueenSide);
        }
    }
}

std::vector<BitMove> Chess::generateAllMoves() {
    // Use current visible board as source state.
    std::string state = stateString();
    char color = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';
    // Return legal moves only for gameplay and UI validation.
    return generateAllMovesFromState(state, color, true);
}

// ============================================================================
// AI METHODS
// ============================================================================

bool Chess::gameHasAI() {
    return getCurrentPlayer() && getCurrentPlayer()->isAIPlayer();
}

void Chess::setAIPlayer(int playerNumber)
{
    // Store which player is AI (0 for white, 1 for black)
    if (playerNumber >= 0 && playerNumber < 2) {
        getPlayerAt(playerNumber)->setAIPlayer(true);
        getPlayerAt((playerNumber + 1) % 2)->setAIPlayer(false);
    } else {
        getPlayerAt(0)->setAIPlayer(false);
        getPlayerAt(1)->setAIPlayer(false);
    }
}

void Chess::stateStringToBitboards(const std::string& state)
{
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
    
    for (size_t i = 0; i < 64 && i < state.length(); i++) {
        char c = state[i];
        if (c == '0') continue;
        
        int playerNumber = isupper(c) ? 0 : 1;
        ChessPiece pieceType;
        
        switch(tolower(c)) {
            case 'p': pieceType = Pawn; break;
            case 'n': pieceType = Knight; break;
            case 'b': pieceType = Bishop; break;
            case 'r': pieceType = Rook; break;
            case 'q': pieceType = Queen; break;
            case 'k': pieceType = King; break;
            default: continue;
        }
        
        SET_BIT(getBitboard(pieceType, playerNumber), static_cast<int>(i));
    }
}

// Check if a given square is attacked by any pieces of the specified color
bool Chess::isSquareAttacked(int square, char byColor) const {
    uint64_t occupied = getAllPieces();
    uint64_t targetMask = (1ULL << square);
    uint64_t enemyPawns = (byColor == 'w') ? _whitePawns : _blackPawns;
    uint64_t enemyKnights = (byColor == 'w') ? _whiteKnights : _blackKnights;
    uint64_t enemyBishops = (byColor == 'w') ? _whiteBishops : _blackBishops;
    uint64_t enemyRooks = (byColor == 'w') ? _whiteRooks : _blackRooks;
    uint64_t enemyQueens = (byColor == 'w') ? _whiteQueens : _blackQueens;
    uint64_t enemyKing = (byColor == 'w') ? _whiteKing : _blackKing;

    // Pawn attacks are directional by attacker color.
    uint64_t pawnAttackers = (byColor == 'w')
                                 ? (SOUTH_EAST(targetMask) | SOUTH_WEST(targetMask))
                                 : (NORTH_EAST(targetMask) | NORTH_WEST(targetMask));
    if (pawnAttackers & enemyPawns) return true;
    if (KnightAttacks[square] & enemyKnights) return true;
    if (KingAttacks[square] & enemyKing) return true;
    if (getBishopAttacks(square, occupied) & (enemyBishops | enemyQueens)) return true;
    if (getRookAttacks(square, occupied) & (enemyRooks | enemyQueens)) return true;

    return false;
}

// Check if the king of the specified color is currently in check by verifying if its square is attacked by any enemy pieces
bool Chess::isKingInCheck(char color) const {
    uint64_t kingBB = (color == 'w') ? _whiteKing : _blackKing;
    if (!kingBB) return true;
    int kingSq = bitScanForward(kingBB);
    return isSquareAttacked(kingSq, color == 'w' ? 'b' : 'w');
}

// Apply a move to a given board state string and return the resulting new state string, handling all special move types (en-passant, castling, promotions)
std::string Chess::applyMoveToState(const std::string& state, const BitMove& move, char moverColor) const {
    std::string newState = state;
    if (move.from >= newState.length() || move.to >= newState.length()) return newState;

    char movingPiece = newState[move.from];
    newState[move.from] = '0';

    // Handle en-passant captured pawn removal.
    if (move.flags & BitMove::MoveEnPassant) {
        int capturedSquare = enPassantCapturedSquare(moverColor, move);
        if (capturedSquare >= 0 && capturedSquare < 64) {
            newState[capturedSquare] = '0';
        }
    }

    // Handle castling rook motion.
    if (move.flags & BitMove::MoveCastleKingSide) {
        int rookFrom, rookTo;
        getCastlingRookFromTo(moverColor, true, rookFrom, rookTo);
        newState[rookTo] = newState[rookFrom];
        newState[rookFrom] = '0';
    } else if (move.flags & BitMove::MoveCastleQueenSide) {
        int rookFrom, rookTo;
        getCastlingRookFromTo(moverColor, false, rookFrom, rookTo);
        newState[rookTo] = newState[rookFrom];
        newState[rookFrom] = '0';
    }

    // Handle promotions (currently auto-queen when promotion flag is set).
    if ((move.flags & BitMove::MovePromotion) && move.promotion != NoPiece) {
        newState[move.to] = promotedCharFromFlag(moverColor, static_cast<ChessPiece>(move.promotion));
    } else {
        newState[move.to] = movingPiece;
    }

    return newState;
}

std::vector<BitMove> Chess::generateAllMovesFromBitboards(const std::string& state, char currentColor, bool legalOnly,
                                                            bool restoreNodeBitboards)
{

    // Optionally save node bitboards so the caller sees no side effects.
    uint64_t savedWhitePawns = 0, savedWhiteKnights = 0, savedWhiteBishops = 0, savedWhiteRooks = 0, savedWhiteQueens = 0, savedWhiteKing = 0;
    uint64_t savedBlackPawns = 0, savedBlackKnights = 0, savedBlackBishops = 0, savedBlackRooks = 0, savedBlackQueens = 0, savedBlackKing = 0;
    if (restoreNodeBitboards) {
        savedWhitePawns = _whitePawns;
        savedWhiteKnights = _whiteKnights;
        savedWhiteBishops = _whiteBishops;
        savedWhiteRooks = _whiteRooks;
        savedWhiteQueens = _whiteQueens;
        savedWhiteKing = _whiteKing;
        savedBlackPawns = _blackPawns;
        savedBlackKnights = _blackKnights;
        savedBlackBishops = _blackBishops;
        savedBlackRooks = _blackRooks;
        savedBlackQueens = _blackQueens;
        savedBlackKing = _blackKing;
    }

    // Generate moves
    std::vector<BitMove> pseudoMoves;
    pseudoMoves.reserve(48);
    
    // Generate pseudo-legal moves from bitboards (including moves that may leave king in check)
    generatePawnMoves(pseudoMoves, currentColor);
    generatePieceMoves(pseudoMoves, currentColor, Knight, knightAttacks);
    generatePieceMoves(pseudoMoves, currentColor, Bishop, bishopAttacks);
    generatePieceMoves(pseudoMoves, currentColor, Rook, rookAttacks);
    generatePieceMoves(pseudoMoves, currentColor, Queen, queenAttacks);
    generatePieceMoves(pseudoMoves, currentColor, King, kingAttacks);
    generateCastlingMoves(pseudoMoves, currentColor);

    std::vector<BitMove> moves;
    if (!legalOnly) {
        moves = pseudoMoves;
    } else {
        // Keep only legal moves (king cannot remain in check after move).
        moves.reserve(pseudoMoves.size());
        for (const auto& move : pseudoMoves) {
            std::string nextState = applyMoveToState(state, move, currentColor);
            stateStringToBitboards(nextState);
            if (!isKingInCheck(currentColor)) {
                moves.push_back(move);
            }
        }
    }

    // Restore node bitboards (critical for correctness when called directly).
    if (restoreNodeBitboards) {
        _whitePawns = savedWhitePawns;
        _whiteKnights = savedWhiteKnights;
        _whiteBishops = savedWhiteBishops;
        _whiteRooks = savedWhiteRooks;
        _whiteQueens = savedWhiteQueens;
        _whiteKing = savedWhiteKing;
        _blackPawns = savedBlackPawns;
        _blackKnights = savedBlackKnights;
        _blackBishops = savedBlackBishops;
        _blackRooks = savedBlackRooks;
        _blackQueens = savedBlackQueens;
        _blackKing = savedBlackKing;
    }

    return moves;
}

std::vector<BitMove> Chess::generateAllMovesFromState(const std::string& state, char currentColor, bool legalOnly)
{
    // Save current bitboards
    uint64_t savedWhitePawns = _whitePawns;
    uint64_t savedWhiteKnights = _whiteKnights;
    uint64_t savedWhiteBishops = _whiteBishops;
    uint64_t savedWhiteRooks = _whiteRooks;
    uint64_t savedWhiteQueens = _whiteQueens;
    uint64_t savedWhiteKing = _whiteKing;
    uint64_t savedBlackPawns = _blackPawns;
    uint64_t savedBlackKnights = _blackKnights;
    uint64_t savedBlackBishops = _blackBishops;
    uint64_t savedBlackRooks = _blackRooks;
    uint64_t savedBlackQueens = _blackQueens;
    uint64_t savedBlackKing = _blackKing;

    // Set up from state string
    stateStringToBitboards(state);
    // Wrapper already restores the caller's bitboards, so we can skip the inner
    // restore to avoid redundant save/restore work.
    std::vector<BitMove> moves = generateAllMovesFromBitboards(state, currentColor, legalOnly, false);

    // Restore saved bitboards
    _whitePawns = savedWhitePawns;
    _whiteKnights = savedWhiteKnights;
    _whiteBishops = savedWhiteBishops;
    _whiteRooks = savedWhiteRooks;
    _whiteQueens = savedWhiteQueens;
    _whiteKing = savedWhiteKing;
    _blackPawns = savedBlackPawns;
    _blackKnights = savedBlackKnights;
    _blackBishops = savedBlackBishops;
    _blackRooks = savedBlackRooks;
    _blackQueens = savedBlackQueens;
    _blackKing = savedBlackKing;

    return moves;
}

int Chess::getPieceValue(char pieceChar) const {
    switch(tolower(pieceChar)) {
        case 'p': return PIECE_VALUES[0];
        case 'n': return PIECE_VALUES[1];
        case 'b': return PIECE_VALUES[2];
        case 'r': return PIECE_VALUES[3];
        case 'q': return PIECE_VALUES[4];
        case 'k': return PIECE_VALUES[5];
        default: return 0;
    }
}

int Chess::getPieceSquareValue(char pieceChar, int square) const {
    bool isWhite = isupper(pieceChar);
    int adjustedSquare = isWhite ? square : (63 - square);
    
    switch(tolower(pieceChar)) {
        case 'p': return pawnTable[adjustedSquare];
        case 'n': return knightTable[adjustedSquare];
        case 'b': return bishopTable[adjustedSquare];
        case 'r': return rookTable[adjustedSquare];
        case 'q': return queenTable[adjustedSquare];
        case 'k': return kingTable[adjustedSquare];
        default: return 0;
    }
}

bool Chess::aiTestForTerminalState(std::string &state, char currentColor, Player *&winner)
{
    if (_whiteKing == 0) {
        winner = getPlayerAt(1);
        return true;
    }
    
    if (_blackKing == 0) {
        winner = getPlayerAt(0);
        return true;
    }
    
    // Check for no legal moves for the side to move in this node.
    std::vector<BitMove> moves = generateAllMovesFromBitboards(state, currentColor, true);
    if (moves.empty()) {
        // No legal moves: checkmate if in check, otherwise stalemate.
        winner = isKingInCheck(currentColor)
                     ? getPlayerAt(currentColor == 'w' ? 1 : 0)
                     : nullptr;
        return true;
    }
    
    return false;
}

// King safety evaluation
int Chess::evaluateKingSafety(char color) {
    uint64_t kingBB = (color == 'w') ? _whiteKing : _blackKing;
    if (kingBB == 0) return 0;
    
    int kingSq = 0;
    uint64_t temp = kingBB;
    while (temp) {
        kingSq = bitScanForward(temp);
        temp &= temp - 1;
    }
    
    int kingFile = kingSq % 8;
    int kingRank = kingSq / 8;
    
    int safety = 0;
    
    // Pawn shield in front of king
    if (color == 'w') {
        if (kingRank < 7) {
            uint64_t pawnShield = _whitePawns & (kingBB << 8);
            if (kingFile > 0) pawnShield |= _whitePawns & (kingBB << 7);
            if (kingFile < 7) pawnShield |= _whitePawns & (kingBB << 9);
            safety += countOnes(pawnShield) * 20;
        }
    } else {
        if (kingRank > 0) {
            uint64_t pawnShield = _blackPawns & (kingBB >> 8);
            if (kingFile > 0) pawnShield |= _blackPawns & (kingBB >> 9);
            if (kingFile < 7) pawnShield |= _blackPawns & (kingBB >> 7);
            safety += countOnes(pawnShield) * 20;
        }
    }
    
    return safety;
}

// Pawn structure evaluation
int Chess::evaluatePawnStructure(char color) {
    uint64_t pawns = (color == 'w') ? _whitePawns : _blackPawns;
    int score = 0;
    
    // Track pawns by file
    int pawnCountByFile[8] = {0};
    
    BitboardElement pawnBB(pawns);
    pawnBB.forEachBit([&](int sq) {
        int file = sq % 8;
        int rank = sq / 8;
        
        pawnCountByFile[file]++;
        
        // Check for passed pawns (no enemy pawns ahead on same or adjacent files)
        // Based on the rank of the pawn, check the appropriate ranks ahead for enemy pawns on the same and adjacent files.
        if (color == 'w') {
            bool isPassed = true;
            for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); f++) {
                uint64_t enemyPawnsOnFile = _blackPawns & (0x0101010101010101ULL << f);
                if (enemyPawnsOnFile & (0xFFFFFFFFFFFFFF00ULL << (rank * 8))) {
                    isPassed = false;
                    break;
                }
            }
            if (isPassed) {
                score += PASSED_PAWN_BONUS[rank];
            }
        } else {
            bool isPassed = true;
            for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); f++) {
                uint64_t enemyPawnsOnFile = _whitePawns & (0x0101010101010101ULL << f);
                if (enemyPawnsOnFile & (0xFFFFFFFFFFFFFF00ULL >> (56 - (rank * 8)))) {
                    isPassed = false;
                    break;
                }
            }
            if (isPassed) {
                score += PASSED_PAWN_BONUS[7 - rank];
            }
        }
    });
    
    // Penalty for doubled pawns (2 or more pawns on the same file)
    for (int file = 0; file < 8; file++) {
        if (pawnCountByFile[file] > 1) {
            score += DOUBLED_PAWN_PENALTY * (pawnCountByFile[file] - 1);
        }
    }
    
    return score;
}

int Chess::evaluateBoard(const std::string &state)
{
    // Parse state into bitboards
    stateStringToBitboards(state);
    
    int score = 0;
    
    // Material and positional evaluation
    for (size_t i = 0; i < 64 && i < state.length(); i++) {
        char piece = state[i];
        if (piece == '0') continue;
        
        bool isWhite = isupper(piece);
        char pieceType = tolower(piece);
        int pieceIndex = 0;
        
        switch(pieceType) {
            case 'p': pieceIndex = 0; break;
            case 'n': pieceIndex = 1; break;
            case 'b': pieceIndex = 2; break;
            case 'r': pieceIndex = 3; break;
            case 'q': pieceIndex = 4; break;
            case 'k': pieceIndex = 5; break;
            default: continue;
        }
        
        int pieceValue = PIECE_VALUES[pieceIndex];
        int positionalValue = getPieceSquareValue(piece, static_cast<int>(i));
        
        // Add center control bonus
        positionalValue += CENTER_BONUS[i];
        
        if (isWhite) {
            score += pieceValue + positionalValue;
        } else {
            score -= (pieceValue + positionalValue);
        }
    }
    
    // Pawn structure evaluation
    score += evaluatePawnStructure('w');
    score -= evaluatePawnStructure('b');
    
    // King safety
    score += evaluateKingSafety('w');
    score -= evaluateKingSafety('b');
    
    // Bishop pair bonus
    if (countOnes(_whiteBishops) >= 2) score += BISHOP_PAIR_BONUS;
    if (countOnes(_blackBishops) >= 2) score -= BISHOP_PAIR_BONUS;
    
    // Mobility evaluation
    std::vector<BitMove> whiteMoves = generateAllMovesFromState(state, 'w');
    std::vector<BitMove> blackMoves = generateAllMovesFromState(state, 'b');
    score += whiteMoves.size() * MOBILITY_BONUS;
    score -= blackMoves.size() * MOBILITY_BONUS;
    
    return score;
}

// Takes the current board state, a move, and the current ply from the root of the search, and 
// returns a score for how promising this move is for move ordering purposes in the search algorithm. 
// Higher scores indicate more promising moves that should be searched first.
int Chess::scoreMoveForOrdering(const std::string& state, const BitMove& move, int plyFromRoot) const {
    int score = 0;
    char moving = (move.from < state.length()) ? state[move.from] : '0';
    char target = (move.to < state.length()) ? state[move.to] : '0';

    // Captures are scored highest, with MVV-LVA (Most Valuable Victim - Least Valuable Attacker) ordering.
    if ((move.flags & BitMove::MoveCapture) || target != '0' || (move.flags & BitMove::MoveEnPassant)) {    // Capture move
        int victimValue = (move.flags & BitMove::MoveEnPassant) ? PIECE_VALUES[0] : getPieceValue(target);  // En-passant captures are treated as capturing a pawn for ordering purposes.
        int attackerValue = getPieceValue(moving);                                                          // MVV-LVA: prioritize captures of more valuable pieces and less valuable attackers.
        score += ORDER_CAPTURE_BASE + (victimValue * 10 - attackerValue);                                   // Bonus for capturing more valuable pieces with less valuable ones.
    } else {
        // Non-capture moves are scored based on killer move heuristics and history heuristic.
        int ply = std::max(0, std::min(plyFromRoot, 15));
        if (_killerFrom[ply][0] == move.from && _killerTo[ply][0] == move.to) score += ORDER_KILLER_BASE;               // Primary killer move for this ply
        else if (_killerFrom[ply][1] == move.from && _killerTo[ply][1] == move.to) score += ORDER_KILLER_BASE - 1000;   // Secondary killer move for this ply
        score += _historyHeuristic[move.from][move.to] * ORDER_HISTORY_SCALE;                                           // Bonus for moves that have historically caused beta cutoffs, scaled by how deep in the search they occur.
    }

    int toRank = move.to / 8;                           // Uses simple heuristic to prioritize moves that control the center of the board
    int toFile = move.to % 8;                           
    int centerDist = abs(3 - toRank) + abs(3 - toFile);
    score += (14 - centerDist);                         // Adds score based on proximity to center
    return score;
}

int Chess::negamax(std::string &state, int depth, int alpha, int beta, 
                   char currentColor, bool wkm, bool bkm, bool wrA, bool wrH, bool brA, bool brH,
                   int epSquare, int plyFromRoot)
{
    // Save search-state metadata and install state for this node (a node is a move).
    bool savedWKM = _whiteKingMoved, savedBKM = _blackKingMoved;
    bool savedWRA = _whiteRookAFileMoved, savedWRH = _whiteRookHFileMoved;
    bool savedBRA = _blackRookAFileMoved, savedBRH = _blackRookHFileMoved;
    int savedEP = _enPassantSquare;
    _whiteKingMoved = wkm; _blackKingMoved = bkm;
    _whiteRookAFileMoved = wrA; _whiteRookHFileMoved = wrH;
    _blackRookAFileMoved = brA; _blackRookHFileMoved = brH;
    _enPassantSquare = epSquare;
    stateStringToBitboards(state); // Bitboards are authoritative for this search node

    Player* winner = nullptr;
    bool isTerminal = aiTestForTerminalState(state, currentColor, winner);
    
    // Test for terminal state (checkmate or stalemate) or depth limit reached, and return appropriate score.
    if (isTerminal || depth == 0) {
        if (isTerminal) {
            if (!winner) {
                _whiteKingMoved = savedWKM; _blackKingMoved = savedBKM;
                _whiteRookAFileMoved = savedWRA; _whiteRookHFileMoved = savedWRH;
                _blackRookAFileMoved = savedBRA; _blackRookHFileMoved = savedBRH;
                _enPassantSquare = savedEP;
                return 0; // Draw
            }
            // Check if current player is winning/losing
            bool currentPlayerWins = (winner->playerNumber() == (currentColor == 'w' ? 0 : 1));
            int terminalScore = currentPlayerWins ? 20000 : -20000;
            _whiteKingMoved = savedWKM; _blackKingMoved = savedBKM;
            _whiteRookAFileMoved = savedWRA; _whiteRookHFileMoved = savedWRH;
            _blackRookAFileMoved = savedBRA; _blackRookHFileMoved = savedBRH;
            _enPassantSquare = savedEP;
            return terminalScore;
        }
        int eval = evaluateBoard(state);
        eval = (currentColor == 'w') ? eval : -eval;
        _whiteKingMoved = savedWKM; _blackKingMoved = savedBKM;
        _whiteRookAFileMoved = savedWRA; _whiteRookHFileMoved = savedWRH;
        _blackRookAFileMoved = savedBRA; _blackRookHFileMoved = savedBRH;
        _enPassantSquare = savedEP;
        return eval;
    }
    
    char nextColor = (currentColor == 'w') ? 'b' : 'w';
    int maxScore = INT_MIN;
    
    std::vector<BitMove> moves = generateAllMovesFromBitboards(state, currentColor, true);
    
    // Move ordering - try captures first
    std::vector<std::pair<BitMove, int>> moveScores;
    for (const auto& move : moves) {
        int moveScore = scoreMoveForOrdering(state, move, plyFromRoot);
        moveScores.push_back({move, moveScore});
    }
    
    std::sort(moveScores.begin(), moveScores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (const auto& moveScore : moveScores) {
        const BitMove& move = moveScore.first;
        
        if (move.to >= state.length() || move.from >= state.length()) continue;
        
        // Apply move and recurse with updated state metadata.
        std::string newState = applyMoveToState(state, move, currentColor);
        int mover = (currentColor == 'w') ? 0 : 1;
        // Snapshot node state so each candidate move starts from identical state.
        NodeSearchState beforeMove = captureNodeSearchState();

        // Apply the move to bitboards to update the node state for the recursive call.
        applyMoveToBitboards(move, mover);
        bool nextWKM = _whiteKingMoved, nextBKM = _blackKingMoved;
        bool nextWRA = _whiteRookAFileMoved, nextWRH = _whiteRookHFileMoved;
        bool nextBRA = _blackRookAFileMoved, nextBRH = _blackRookHFileMoved;
        int nextEP = _enPassantSquare;

        int score = -negamax(newState, depth - 1, -beta, -alpha, nextColor,
                             nextWKM, nextBKM, nextWRA, nextWRH, nextBRA, nextBRH, nextEP, plyFromRoot + 1);
        
        // Restore node metadata/bitboards for the next candidate move.
        restoreNodeSearchState(beforeMove);
        
        if (score > maxScore) {
            maxScore = score;
        }
        
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            if (!((move.flags & BitMove::MoveCapture) || (move.flags & BitMove::MoveEnPassant))) {
                int ply = std::max(0, std::min(plyFromRoot, 15));
                _killerFrom[ply][1] = _killerFrom[ply][0];
                _killerTo[ply][1] = _killerTo[ply][0];
                _killerFrom[ply][0] = move.from;
                _killerTo[ply][0] = move.to;
                _historyHeuristic[move.from][move.to] += depth * depth;
            }
            break; // Beta cutoff
        }
    }

    int result = (maxScore == INT_MIN) ? 0 : maxScore;
    _whiteKingMoved = savedWKM; _blackKingMoved = savedBKM;
    _whiteRookAFileMoved = savedWRA; _whiteRookHFileMoved = savedWRH;
    _blackRookAFileMoved = savedBRA; _blackRookHFileMoved = savedBRH;
    _enPassantSquare = savedEP;
    return result;
}

void Chess::updateAI()
{
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!gameHasAI() || !_grid) return;
    
    std::string state = stateString();
    stateStringToBitboards(state); // Root synchronization for deterministic search.
    int depth = _searchDepth;
    
    int bestScore = INT_MIN;
    
    int aiPlayerNum = getCurrentPlayer()->playerNumber();
    char aiColor = (aiPlayerNum == 0) ? 'w' : 'b';
    char opponentColor = (aiPlayerNum == 0) ? 'b' : 'w';
    
    std::vector<BitMove> moves = generateAllMovesFromBitboards(state, aiColor, true);
    
    // If no moves available, return
    if (moves.empty()) return;
    
    for (const auto& move : moves) {
        std::string testState = applyMoveToState(state, move, aiColor);

        NodeSearchState beforeMove = captureNodeSearchState();
        applyMoveToBitboards(move, aiPlayerNum);

        // Negamax with alpha-beta pruning + legal move generation.
        int score = -negamax(testState, depth - 1, INT_MIN + 1, INT_MAX, opponentColor,
                             _whiteKingMoved, _blackKingMoved,
                             _whiteRookAFileMoved, _whiteRookHFileMoved,
                             _blackRookAFileMoved, _blackRookHFileMoved,
                             _enPassantSquare, 1);
        restoreNodeSearchState(beforeMove);
        
        if (score > bestScore) {
            bestScore = score;
            _bestMove = move;
        }
    }
    
    if (_bestMove.piece != NoPiece) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        makeMove(_bestMove);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

// Helper function for bit scanning
int Chess::bitScanForward(uint64_t bb) const {
#ifdef _MSC_VER
    unsigned long index;
    _BitScanForward64(&index, bb);
    return index;
#else
    return __builtin_ctzll(bb);
#endif
}