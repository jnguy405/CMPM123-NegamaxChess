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

// Piece values (adjusted for better play)
const int PIECE_VALUES[6] = {
    100,   // Pawn
    320,   // Knight  
    330,   // Bishop
    500,   // Rook
    900,   // Queen
    20000  // King
};

// Bonus for controlling center squares
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

// Bonus for bishop pair
const int BISHOP_PAIR_BONUS = 30;

// Mobility bonus per move
const int MOBILITY_BONUS = 2;

// Move ordering - center squares first for better pruning
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

// ============================================================================
// CONSTRUCTION & INITIALIZATION
// ============================================================================

Chess::Chess()
    : _whitePawns(0), _whiteKnights(0), _whiteBishops(0), _whiteRooks(0), _whiteQueens(0), _whiteKing(0),
      _blackPawns(0), _blackKnights(0), _blackBishops(0), _blackRooks(0), _blackQueens(0), _blackKing(0)
{
    _grid = new Grid(8, 8);
    _bestMove = BitMove();
}

Chess::~Chess()
{
    delete _grid;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;
    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
    startGame();
}

void Chess::stopGame() {
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
    
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
}

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

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    int currentPlayerNum = getCurrentPlayer()->playerNumber();
    int pieceColor = (bit.gameTag() >= 128) ? 1 : 0;
    return (pieceColor == currentPlayerNum);
}

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

void Chess::applyMoveToBitboards(const BitMove& move, int playerNumber) {
    ChessPiece pieceType = static_cast<ChessPiece>(move.piece);
    int enemyPlayer = (playerNumber == 0) ? 1 : 0;
    
    CLEAR_BIT(getBitboard(pieceType, playerNumber), move.from);
    SET_BIT(getBitboard(pieceType, playerNumber), move.to);
    
    static const ChessPiece types[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
    for (int i = 0; i < 6; i++) {
        CLEAR_BIT(getBitboard(types[i], enemyPlayer), move.to);
    }
}

void Chess::makeMove(const BitMove& move) {
    updateBitboardsFromGrid();
    
    int currentPlayer = getCurrentPlayer()->playerNumber();
    applyMoveToBitboards(move, currentPlayer);
    
    updateGridFromBitboards();
    endTurn();
}

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

Player* Chess::checkForWinner() {
    updateBitboardsFromGrid();
    if (_whiteKing == 0) return getPlayerAt(1);
    if (_blackKing == 0) return getPlayerAt(0);
    return nullptr;
}

bool Chess::checkForDraw() {
    return generateAllMoves().empty();
}

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

void Chess::addPawnBitboardMovesToList(std::vector<BitMove>& moves, uint64_t bitboard, int shift) {
    BitboardElement(bitboard).forEachBit([&](int toSquare) {
        int fromSquare = toSquare - shift;
        moves.emplace_back(fromSquare, toSquare, Pawn);
    });
}

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

    addPawnBitboardMovesToList(moves, singleMoves,   shiftForward);
    addPawnBitboardMovesToList(moves, doubleMoves,   shiftDouble);
    addPawnBitboardMovesToList(moves, capturesLeft,  shiftCaptureLeft);
    addPawnBitboardMovesToList(moves, capturesRight, shiftCaptureRight);
}

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
            moves.emplace_back(fromSquare, toSquare, pieceType);
        });
    });
}

std::vector<BitMove> Chess::generateAllMoves() {
    std::vector<BitMove> moves;
    moves.reserve(40);

    updateBitboardsFromGrid();

    char color = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';

    generatePawnMoves(moves, color);
    generatePieceMoves(moves, color, Knight, knightAttacks);
    generatePieceMoves(moves, color, Bishop, bishopAttacks);
    generatePieceMoves(moves, color, Rook, rookAttacks);
    generatePieceMoves(moves, color, Queen, queenAttacks);
    generatePieceMoves(moves, color, King, kingAttacks);

    return moves;
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

std::vector<BitMove> Chess::generateAllMovesFromState(const std::string& state, char currentColor)
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
    
    // Generate moves
    std::vector<BitMove> moves;
    moves.reserve(40);
    
    generatePawnMoves(moves, currentColor);
    generatePieceMoves(moves, currentColor, Knight, knightAttacks);
    generatePieceMoves(moves, currentColor, Bishop, bishopAttacks);
    generatePieceMoves(moves, currentColor, Rook, rookAttacks);
    generatePieceMoves(moves, currentColor, Queen, queenAttacks);
    generatePieceMoves(moves, currentColor, King, kingAttacks);
    
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

bool Chess::aiTestForTerminalState(std::string &state, Player *&winner)
{
    stateStringToBitboards(state);
    
    if (_whiteKing == 0) {
        winner = getPlayerAt(1);
        return true;
    }
    
    if (_blackKing == 0) {
        winner = getPlayerAt(0);
        return true;
    }
    
    // Check for stalemate (no legal moves)
    char currentColor = 'w';
    for (size_t i = 0; i < state.length(); i++) {
        if (state[i] != '0') {
            currentColor = isupper(state[i]) ? 'w' : 'b';
            break;
        }
    }
    
    std::vector<BitMove> moves = generateAllMovesFromState(state, currentColor);
    if (moves.empty()) {
        winner = nullptr; // Stalemate is a draw
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
    
    // Penalty for doubled pawns
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

int Chess::negamax(std::string &state, int depth, int alpha, int beta, 
                   char currentColor)
{
    Player* winner = nullptr;
    bool isTerminal = aiTestForTerminalState(state, winner);
    
    if (isTerminal || depth == 0) {
        if (isTerminal) {
            if (!winner) return 0; // Draw
            // Check if current player is winning/losing
            bool currentPlayerWins = (winner->playerNumber() == (currentColor == 'w' ? 0 : 1));
            return currentPlayerWins ? 20000 : -20000;
        }
        return evaluateBoard(state);
    }
    
    char nextColor = (currentColor == 'w') ? 'b' : 'w';
    int maxScore = INT_MIN;
    
    std::vector<BitMove> moves = generateAllMovesFromState(state, currentColor);
    
    // Move ordering - try captures first
    std::vector<std::pair<BitMove, int>> moveScores;
    for (const auto& move : moves) {
        int moveScore = 0;
        if (move.to < state.length() && move.from < state.length()) {
            char targetPiece = state[move.to];
            if (targetPiece != '0') {
                moveScore = getPieceValue(targetPiece) * 10;
            }
            // Prioritize center control
            int toRank = move.to / 8;
            int toFile = move.to % 8;
            int centerDist = abs(3 - toRank) + abs(3 - toFile);
            moveScore += (14 - centerDist);
        }
        moveScores.push_back({move, moveScore});
    }
    
    std::sort(moveScores.begin(), moveScores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (const auto& moveScore : moveScores) {
        const BitMove& move = moveScore.first;
        
        if (move.to >= state.length() || move.from >= state.length()) continue;
        
        // Make move
        std::string newState = state;
        newState[move.to] = newState[move.from];
        newState[move.from] = '0';
        
        int score = -negamax(newState, depth - 1, -beta, -alpha, nextColor);
        
        if (score > maxScore) {
            maxScore = score;
        }
        
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break; // Beta cutoff
        }
    }
    
    return (maxScore == INT_MIN) ? 0 : maxScore;
}

void Chess::updateAI()
{
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!gameHasAI() || !_grid) return;
    
    std::string state = stateString();
    int depth = 3; // Depth of 3 as required
    
    int bestScore = INT_MIN;
    
    int aiPlayerNum = getCurrentPlayer()->playerNumber();
    char aiColor = (aiPlayerNum == 0) ? 'w' : 'b';
    char opponentColor = (aiPlayerNum == 0) ? 'b' : 'w';
    
    std::vector<BitMove> moves = generateAllMoves();
    
    // If no moves available, return
    if (moves.empty()) return;
    
    for (const auto& move : moves) {
        // Make move on state
        std::string testState = state;
        testState[move.to] = testState[move.from];
        testState[move.from] = '0';
        
        // Negamax with alpha-beta pruning
        int score = -negamax(testState, depth - 1, INT_MIN + 1, INT_MAX, opponentColor);
        
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