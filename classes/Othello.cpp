#include "Othello.h"
#include <iostream>

// Define the 8 directions: N, NE, E, SE, S, SW, W, NW
const int Othello::DIRECTIONS[8][2] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1},
    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
};

Othello::Othello() : Game() {
    _grid = new Grid(8, 8);
    _consecutivePasses = 0;
    _showingHints = false;
}

Othello::~Othello() {
    delete _grid;
}

void Othello::setUpBoard() {
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeSquares(80, "boardsquare.png");

    // Set up initial four pieces in the center
    Player* blackPlayer = getPlayerAt(BLACK_PLAYER);
    Player* whitePlayer = getPlayerAt(WHITE_PLAYER);

    // Standard Othello starting position
    auto placePiece = [&](int x, int y, Player* player) {
        Bit* piece = createPiece(player);
        piece->setPosition(_grid->getSquare(x, y)->getPosition());
        _grid->getSquare(x, y)->setBit(piece);
    };

    placePiece(3, 3, whitePlayer);  // White at (3,3)
    placePiece(4, 4, whitePlayer);  // White at (4,4)
    placePiece(4, 3, blackPlayer);  // Black at (4,3)
    placePiece(3, 4, blackPlayer);  // Black at (3,4)

    if (gameHasAI()) {
        setAIPlayer(AI_PLAYER);
    }
    
    startGame();
}

Bit* Othello::createPiece(Player* player) {
    Bit* bit = new Bit();
    bit->LoadTextureFromFile(player == getPlayerAt(BLACK_PLAYER) ? "o.png" : "x.png");
    bit->setOwner(player);
    return bit;
}

bool Othello::actionForEmptyHolder(BitHolder &holder) {
    if (holder.bit()) return false;

    ChessSquare* square = static_cast<ChessSquare*>(&holder);
    int x = square->getColumn();
    int y = square->getRow();
    Player* currentPlayer = getCurrentPlayer();

    if (!isValidMove(x, y, currentPlayer)) return false;

    // Place the piece
    Bit* newPiece = createPiece(currentPlayer);
    newPiece->setPosition(holder.getPosition());
    holder.setBit(newPiece);

    // Flip all affected pieces
    flipPieces(x, y, currentPlayer);
    _consecutivePasses = 0;

    // Check if next player has moves
    Player* nextPlayer = getPlayerAt(1 - currentPlayer->playerNumber());
    if (!hasValidMove(nextPlayer)) {
        _consecutivePasses++;
        if (hasValidMove(currentPlayer)) {
            // Next player passes, current player continues
            return true;
        } else {
            _consecutivePasses = 2; // Game ends
        }
    }

    endTurn();
    return true;
}

bool Othello::canBitMoveFrom(Bit &bit, BitHolder &src) {
    return false; // Pieces cannot be moved in Othello
}

bool Othello::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) {
    return false; // Pieces cannot be moved in Othello
}

bool Othello::isValidMove(int x, int y, Player* player) const {
    if (!_grid->isValid(x, y) || _grid->getSquare(x, y)->bit()) return false;

    // Check if placing a piece here would flip at least one opponent piece
    for (int i = 0; i < 8; i++) {
        if (checkDirection(x, y, DIRECTIONS[i][0], DIRECTIONS[i][1], player) > 0) {
            return true;
        }
    }
    return false;
}

int Othello::checkDirection(int x, int y, int dx, int dy, Player* player) const {
    int count = 0;
    int nx = x + dx;
    int ny = y + dy;

    if (!_grid->isValid(nx, ny)) return 0;

    Bit* firstPiece = _grid->getSquare(nx, ny)->bit();
    if (!firstPiece || firstPiece->getOwner() == player) return 0;

    // Count opponent pieces in this direction
    while (_grid->isValid(nx, ny)) {
        Bit* piece = _grid->getSquare(nx, ny)->bit();
        if (!piece) return 0;
        if (piece->getOwner() == player) return count;
        count++;
        nx += dx;
        ny += dy;
    }
    return 0;
}

void Othello::flipPieces(int x, int y, Player* player) {
    for (int i = 0; i < 8; i++) {
        int count = checkDirection(x, y, DIRECTIONS[i][0], DIRECTIONS[i][1], player);
        if (count > 0) {
            flipInDirection(x, y, DIRECTIONS[i][0], DIRECTIONS[i][1], player, count);
        }
    }
}

void Othello::flipInDirection(int x, int y, int dx, int dy, Player* player, int count) {
    int nx = x + dx;
    int ny = y + dy;

    for (int i = 0; i < count; i++) {
        ChessSquare* square = _grid->getSquare(nx, ny);
        if (square && square->bit()) {
            square->destroyBit();
            Bit* newPiece = createPiece(player);
            newPiece->setPosition(square->getPosition());
            square->setBit(newPiece);
        }
        nx += dx;
        ny += dy;
    }
}

bool Othello::hasValidMove(Player* player) const {
    bool hasMove = false;
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        if (!hasMove && isValidMove(x, y, player)) {
            hasMove = true;
        }
    });
    return hasMove;
}

std::vector<std::pair<int, int>> Othello::getValidMoves(Player* player) const {
    std::vector<std::pair<int, int>> moves;
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        if (isValidMove(x, y, player)) {
            moves.push_back({x, y});
        }
    });
    return moves;
}

Player* Othello::checkForWinner() {
    // Game ends when neither player can move
    if (_consecutivePasses >= 2 ||
        (!hasValidMove(getPlayerAt(BLACK_PLAYER)) && !hasValidMove(getPlayerAt(WHITE_PLAYER)))) {

        int blackCount, whiteCount;
        countPieces(blackCount, whiteCount);

        if (blackCount > whiteCount) return getPlayerAt(BLACK_PLAYER);
        if (whiteCount > blackCount) return getPlayerAt(WHITE_PLAYER);
    }

    // Check if board is full
    bool boardFull = true;
    _grid->forEachSquare([&boardFull](ChessSquare* square, int x, int y) {
        if (!square->bit()) boardFull = false;
    });

    if (boardFull) {
        int blackCount, whiteCount;
        countPieces(blackCount, whiteCount);
        if (blackCount > whiteCount) return getPlayerAt(BLACK_PLAYER);
        if (whiteCount > blackCount) return getPlayerAt(WHITE_PLAYER);
    }

    return nullptr;
}

bool Othello::checkForDraw() {
    if (_consecutivePasses >= 2 ||
        (!hasValidMove(getPlayerAt(BLACK_PLAYER)) && !hasValidMove(getPlayerAt(WHITE_PLAYER)))) {
        int blackCount, whiteCount;
        countPieces(blackCount, whiteCount);
        return blackCount == whiteCount;
    }

    bool boardFull = true;
    _grid->forEachSquare([&boardFull](ChessSquare* square, int x, int y) {
        if (!square->bit()) boardFull = false;
    });

    if (boardFull) {
        int blackCount, whiteCount;
        countPieces(blackCount, whiteCount);
        return blackCount == whiteCount;
    }
    return false;
}

void Othello::countPieces(int &blackCount, int &whiteCount) const {
    blackCount = 0;
    whiteCount = 0;

    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        Bit* piece = square->bit();
        if (piece) {
            if (piece->getOwner() == getPlayerAt(BLACK_PLAYER)) {
                blackCount++;
            } else {
                whiteCount++;
            }
        }
    });
}

void Othello::stopGame() {
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
    _consecutivePasses = 0;
}

std::string Othello::initialStateString() {
    std::string state(64, '0');
    state[3 * 8 + 3] = '2';  // White at (3,3)
    state[4 * 8 + 4] = '2';  // White at (4,4)
    state[4 * 8 + 3] = '1';  // Black at (4,3)
    state[3 * 8 + 4] = '1';  // Black at (3,4)
    return state;
}

std::string Othello::stateString() {
    std::string state;
    _grid->forEachSquare([&state, this](ChessSquare* square, int x, int y) {
        Bit* bit = square->bit();
        if (!bit) {
            state += '0';
        } else if (bit->getOwner() == getPlayerAt(BLACK_PLAYER)) {
            state += '1';
        } else {
            state += '2';
        }
    });
    return state;
}

void Othello::setStateString(const std::string &s) {
    if (s.length() != 64) return;

    int index = 0;
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        if (index < s.length()) {
            char pieceType = s[index++];
            square->destroyBit();

            if (pieceType == '1') {
                Bit* piece = createPiece(getPlayerAt(BLACK_PLAYER));
                piece->setPosition(square->getPosition());
                square->setBit(piece);
            } else if (pieceType == '2') {
                Bit* piece = createPiece(getPlayerAt(WHITE_PLAYER));
                piece->setPosition(square->getPosition());
                square->setBit(piece);
            }
        }
    });
}

void Othello::updateAI() {
    if (!gameHasAI()) return;

    Player* aiPlayer = getCurrentPlayer();
    std::vector<std::pair<int, int>> validMoves = getValidMoves(aiPlayer);

    if (validMoves.empty()) {
        _consecutivePasses++;
        endTurn();
        return;
    }

    // Find move that flips the most pieces
    int bestX = -1, bestY = -1, maxFlips = 0;

    for (const auto& move : validMoves) {
        int x = move.first, y = move.second, totalFlips = 0;
        for (int i = 0; i < 8; i++) {
            totalFlips += checkDirection(x, y, DIRECTIONS[i][0], DIRECTIONS[i][1], aiPlayer);
        }
        if (totalFlips > maxFlips) {
            maxFlips = totalFlips;
            bestX = x;
            bestY = y;
        }
    }

    if (bestX >= 0 && bestY >= 0) {
        actionForEmptyHolder(*_grid->getSquare(bestX, bestY));
    }
}

void Othello::getBoardPosition(BitHolder& holder, int &x, int &y) const {
    ChessSquare* square = static_cast<ChessSquare*>(&holder);
    x = square->getColumn();
    y = square->getRow();
}

void Othello::showValidMoves(Player* player) {
    _showingHints = true;
}

void Othello::clearValidMoveIndicators() {
    _showingHints = false;
}