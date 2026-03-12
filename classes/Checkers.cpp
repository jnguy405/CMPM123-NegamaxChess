#include "Checkers.h"

Checkers::Checkers() : Game() {
    _grid = new Grid(8, 8);
    _mustContinueJumping = false;
    _jumpingPiece = nullptr;
    _redPieces = 12;
    _yellowPieces = 12;
}

Checkers::~Checkers() {
    delete _grid;
}

void Checkers::setUpBoard() {
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    // Initialize all squares
    _grid->initializeSquares(80, "boardsquare.png");

    // Enable only dark squares and place pieces
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        bool isDark = (x + y) % 2 == 1;
        _grid->setEnabled(x, y, isDark);

        if (isDark) {
            if (y < 3) {
                Bit* piece = createPiece(RED_PIECE);
                piece->setPosition(square->getPosition());
                square->setBit(piece);
            } else if (y > 4) {
                Bit* piece = createPiece(YELLOW_PIECE);
                piece->setPosition(square->getPosition());
                square->setBit(piece);
            }
        }
    });

    startGame();
}

Bit* Checkers::createPiece(int pieceType) {
    Bit* bit = new Bit();
    bool isRed = (pieceType == RED_PIECE || pieceType == RED_KING);
    bit->LoadTextureFromFile(isRed ? "red.png" : "yellow.png");
    bit->setOwner(getPlayerAt(isRed ? RED_PLAYER : YELLOW_PLAYER));
    bit->setGameTag(pieceType);
    if (pieceType == RED_KING || pieceType == YELLOW_KING)
        bit->setScale(1.3f);
    return bit;
}

bool Checkers::actionForEmptyHolder(BitHolder &holder) {
    return false; // Checkers doesn't place new pieces
}

bool Checkers::canBitMoveFrom(Bit &bit, BitHolder &src) {
    if (!src.bit() || bit.getOwner() != getCurrentPlayer()) return false;
    if (_mustContinueJumping && &src != _jumpingPiece) return false;

    ChessSquare* square = static_cast<ChessSquare*>(&src);
    int x = square->getColumn();
    int y = square->getRow();

    // Must jump if available
    if (hasJumpAvailable(bit.getOwner())) {
        return canJumpFrom(*square);
    }
    return true;
}

bool Checkers::canBitMoveFromTo(Bit& bit, BitHolder& src, BitHolder& dst) {
    if (!src.bit() || dst.bit()) return false;

    ChessSquare* srcSquare = static_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = static_cast<ChessSquare*>(&dst);

    int srcX = srcSquare->getColumn();
    int srcY = srcSquare->getRow();

    if (!_grid->isEnabled(dstSquare->getColumn(), dstSquare->getRow())) return false;

    bool isKing = (bit.gameTag() == RED_KING || bit.gameTag() == YELLOW_KING);
    bool isRed = (bit.getOwner() == getPlayerAt(RED_PLAYER));

    // Simple moves (if no jumps required)
    if (!_mustContinueJumping && !hasJumpAvailable(bit.getOwner())) {
        if (isKing) {
            return dstSquare == _grid->getFL(srcX, srcY) || dstSquare == _grid->getFR(srcX, srcY) ||
                   dstSquare == _grid->getBL(srcX, srcY) || dstSquare == _grid->getBR(srcX, srcY);
        }
        return isRed ? (dstSquare == _grid->getBL(srcX, srcY) || dstSquare == _grid->getBR(srcX, srcY)) :
                       (dstSquare == _grid->getFL(srcX, srcY) || dstSquare == _grid->getFR(srcX, srcY));
    }

    // Jump moves
    if (_mustContinueJumping && &src != _jumpingPiece) return false;

    // Check all jump directions
    auto checkJump = [&](ChessSquare* middle, ChessSquare* target) -> bool {
        if (!middle || !target || !middle->bit()) return false;
        if (middle->bit()->getOwner() == bit.getOwner()) return false;
        return dstSquare == target;
    };

    if (isKing || !isRed) {
        if (checkJump(_grid->getFL(srcX, srcY), _grid->getFLFL(srcX, srcY))) return true;
        if (checkJump(_grid->getFR(srcX, srcY), _grid->getFRFR(srcX, srcY))) return true;
    }
    if (isKing || isRed) {
        if (checkJump(_grid->getBL(srcX, srcY), _grid->getBLBL(srcX, srcY))) return true;
        if (checkJump(_grid->getBR(srcX, srcY), _grid->getBRBR(srcX, srcY))) return true;
    }

    return false;
}

void Checkers::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) {
    ChessSquare* srcSquare = static_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = static_cast<ChessSquare*>(&dst);

    int srcX = srcSquare->getColumn();
    int srcY = srcSquare->getRow();
    int dstX = dstSquare->getColumn();
    int dstY = dstSquare->getRow();

    // Check for jump
    ChessSquare* jumped = nullptr;
    if (dstSquare == _grid->getFLFL(srcX, srcY)) jumped = _grid->getFL(srcX, srcY);
    else if (dstSquare == _grid->getFRFR(srcX, srcY)) jumped = _grid->getFR(srcX, srcY);
    else if (dstSquare == _grid->getBLBL(srcX, srcY)) jumped = _grid->getBL(srcX, srcY);
    else if (dstSquare == _grid->getBRBR(srcX, srcY)) jumped = _grid->getBR(srcX, srcY);

    if (jumped && jumped->bit()) {
        // Capture
        (jumped->bit()->getOwner() == getPlayerAt(RED_PLAYER)) ? _redPieces-- : _yellowPieces--;
        jumped->destroyBit();

        // Promotion check
        if ((bit.gameTag() == RED_PIECE && dstY == 7) || (bit.gameTag() == YELLOW_PIECE && dstY == 0)) {
            bit.setGameTag(bit.gameTag() == RED_PIECE ? RED_KING : YELLOW_KING);
            bit.setScale(1.3f);
        }

        // Check for more jumps
        if (canJumpFrom(*dstSquare)) {
            _mustContinueJumping = true;
            _jumpingPiece = &dst;
            return;
        }
    } else {
        // Regular move - promotion check
        if ((bit.gameTag() == RED_PIECE && dstY == 7) || (bit.gameTag() == YELLOW_PIECE && dstY == 0)) {
            bit.setGameTag(bit.gameTag() == RED_PIECE ? RED_KING : YELLOW_KING);
            bit.setScale(1.3f);
        }
    }

    _mustContinueJumping = false;
    _jumpingPiece = nullptr;
    endTurn();
}

bool Checkers::canJumpFrom(ChessSquare& square) const {
    Bit* piece = square.bit();
    if (!piece) return false;

    int x = square.getColumn();
    int y = square.getRow();
    bool isKing = (piece->gameTag() == RED_KING || piece->gameTag() == YELLOW_KING);
    bool isRed = (piece->getOwner() == getPlayerAt(RED_PLAYER));
    Player* player = piece->getOwner();

    auto checkJumpDir = [&](ChessSquare* middle, ChessSquare* target) -> bool {
        return middle && middle->bit() && middle->bit()->getOwner() != player &&
               target && !target->bit();
    };

    if (isKing || !isRed) {
        if (checkJumpDir(_grid->getFL(x, y), _grid->getFLFL(x, y))) return true;
        if (checkJumpDir(_grid->getFR(x, y), _grid->getFRFR(x, y))) return true;
    }
    if (isKing || isRed) {
        if (checkJumpDir(_grid->getBL(x, y), _grid->getBLBL(x, y))) return true;
        if (checkJumpDir(_grid->getBR(x, y), _grid->getBRBR(x, y))) return true;
    }
    return false;
}

bool Checkers::hasJumpAvailable(Player* player) const {
    bool hasJump = false;
    _grid->forEachEnabledSquare([&](ChessSquare* square, int x, int y) {
        if (hasJump) return;
        Bit* piece = square->bit();
        if (piece && piece->getOwner() == player) {
            Checkers* self = const_cast<Checkers*>(this);
            if (self->canJumpFrom(*square)) {
                hasJump = true;
            }
        }
    });
    return hasJump;
}

Player* Checkers::checkForWinner() {
    if (_redPieces == 0) return getPlayerAt(YELLOW_PLAYER);
    if (_yellowPieces == 0) return getPlayerAt(RED_PLAYER);

    // Check if current player has any moves
    Player* current = getCurrentPlayer();
    bool hasMove = false;

    _grid->forEachEnabledSquare([&](ChessSquare* square, int x, int y) {
        if (hasMove) return;
        Bit* piece = square->bit();
        if (piece && piece->getOwner() == current) {
            bool isKing = (piece->gameTag() == RED_KING || piece->gameTag() == YELLOW_KING);
            bool isRed = (current == getPlayerAt(RED_PLAYER));

            // Check for any valid move
            auto checkMove = [&](ChessSquare* target) {
                if (target && !target->bit()) hasMove = true;
            };

            if (isKing) {
                checkMove(_grid->getFL(x, y));
                checkMove(_grid->getFR(x, y));
                checkMove(_grid->getBL(x, y));
                checkMove(_grid->getBR(x, y));
            } else if (isRed) {
                checkMove(_grid->getBL(x, y));
                checkMove(_grid->getBR(x, y));
            } else {
                checkMove(_grid->getFL(x, y));
                checkMove(_grid->getFR(x, y));
            }
        }
    });

    if (!hasMove) {
        return current == getPlayerAt(RED_PLAYER) ? getPlayerAt(YELLOW_PLAYER) : getPlayerAt(RED_PLAYER);
    }
    return nullptr;
}

bool Checkers::checkForDraw() {
    return false;
}

void Checkers::stopGame() {
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
    _mustContinueJumping = false;
    _jumpingPiece = nullptr;
    _redPieces = 12;
    _yellowPieces = 12;
}

std::string Checkers::initialStateString() {
    return "111111111111--------333333333333";
}

std::string Checkers::stateString() {
    return _grid->getStateString();
}

void Checkers::setStateString(const std::string &s) {
    if (s.length() != 32) return;

    _redPieces = 0;
    _yellowPieces = 0;

    _grid->setStateString(s);

    // Recreate pieces from state
    size_t index = 0;
    _grid->forEachEnabledSquare([&](ChessSquare* square, int x, int y) {
        if (index < s.length()) {
            int pieceType = s[index++] - '0';
            if (pieceType != 0) {
                Bit* piece = createPiece(pieceType);
                piece->setPosition(square->getPosition());
                square->setBit(piece);
                (pieceType == RED_PIECE || pieceType == RED_KING) ? _redPieces++ : _yellowPieces++;
            }
        }
    });
}

void Checkers::updateAI() {}

