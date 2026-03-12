#include "Grid.h"

Grid::Grid(int width, int height) : _width(width), _height(height)
{
    // Initialize 2D vectors
    _squares.resize(height);
    _enabled.resize(height);

    for (int y = 0; y < height; y++) {
        _squares[y].resize(width);
        _enabled[y].resize(width);

        for (int x = 0; x < width; x++) {
            _squares[y][x] = new ChessSquare();
            _enabled[y][x] = true; // All squares enabled by default
        }
    }
}

Grid::~Grid()
{
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            delete _squares[y][x];
        }
    }
}

ChessSquare* Grid::getSquare(int x, int y)
{
    if (!isValid(x, y)) return nullptr;
    return _squares[y][x];
}

ChessSquare* Grid::getSquareByIndex(int index)
{
    int x, y;
    getCoordinates(index, x, y);
    return getSquare(x, y);
}

bool Grid::isValid(int x, int y) const
{
    return x >= 0 && x < _width && y >= 0 && y < _height;
}

bool Grid::isEnabled(int x, int y) const
{
    if (!isValid(x, y)) return false;
    return _enabled[y][x];
}

void Grid::setEnabled(int x, int y, bool enabled)
{
    if (isValid(x, y)) {
        _enabled[y][x] = enabled;
    }
}

void Grid::getCoordinates(int index, int& x, int& y) const
{
    x = index % _width;
    y = index / _width;
}

// Directional helpers
ChessSquare* Grid::getFL(int x, int y)
{
    // Front-left: up and left (row decreases, column decreases)
    return getSquare(x - 1, y - 1);
}

ChessSquare* Grid::getFR(int x, int y)
{
    // Front-right: up and right (row decreases, column increases)
    return getSquare(x + 1, y - 1);
}

ChessSquare* Grid::getBL(int x, int y)
{
    // Back-left: down and left (row increases, column decreases)
    return getSquare(x - 1, y + 1);
}

ChessSquare* Grid::getBR(int x, int y)
{
    // Back-right: down and right (row increases, column increases)
    return getSquare(x + 1, y + 1);
}

ChessSquare* Grid::getN(int x, int y)
{
    return getSquare(x, y - 1);
}

ChessSquare* Grid::getS(int x, int y)
{
    return getSquare(x, y + 1);
}

ChessSquare* Grid::getE(int x, int y)
{
    return getSquare(x + 1, y);
}

ChessSquare* Grid::getW(int x, int y)
{
    return getSquare(x - 1, y);
}

// Graph connections
void Grid::addConnection(int fromIndex, int toIndex)
{
    _connections[fromIndex].push_back(toIndex);
}

void Grid::addConnection(int fromX, int fromY, int toX, int toY)
{
    addConnection(getIndex(fromX, fromY), getIndex(toX, toY));
}

std::vector<ChessSquare*> Grid::getConnectedSquares(int x, int y)
{
    std::vector<ChessSquare*> connected;
    int index = getIndex(x, y);

    if (_connections.find(index) != _connections.end()) {
        for (int connectedIndex : _connections[index]) {
            ChessSquare* square = getSquareByIndex(connectedIndex);
            if (square) {
                connected.push_back(square);
            }
        }
    }

    return connected;
}

bool Grid::areConnected(int fromX, int fromY, int toX, int toY)
{
    int fromIndex = getIndex(fromX, fromY);
    int toIndex = getIndex(toX, toY);

    if (_connections.find(fromIndex) != _connections.end()) {
        const auto& connections = _connections[fromIndex];
        return std::find(connections.begin(), connections.end(), toIndex) != connections.end();
    }

    return false;
}

// Iterator support
void Grid::forEachSquare(std::function<void(ChessSquare*, int x, int y)> func)
{
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            func(_squares[y][x], x, y);
        }
    }
}

void Grid::forEachEnabledSquare(std::function<void(ChessSquare*, int x, int y)> func)
{
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            if (_enabled[y][x]) {
                func(_squares[y][x], x, y);
            }
        }
    }
}

// Initialize squares
void Grid::initializeSquares(float squareSize, const char* spriteName)
{
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            initializeSquare(x, y, squareSize, spriteName);
        }
    }
}

// chess board starts at bottom a1 = 0,0
void Grid::initializeChessSquares(float squareSize, const char* spriteName)
{
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            ImVec2 position(squareSize * x + squareSize/2, squareSize * (7-y) + squareSize/2);
            _squares[y][x]->initHolder(position, spriteName, x, y);
        }
    }
}

void Grid::initializeSquare(int x, int y, float squareSize, const char* spriteName)
{
    if (isValid(x, y)) {
        ImVec2 position(squareSize * x + squareSize/2, squareSize * y + squareSize/2);
        _squares[y][x]->initHolder(position, spriteName, x, y);
    }
}

// State management
std::string Grid::getStateString() const
{
    std::string state;

    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            if (_enabled[y][x]) {
                Bit* bit = _squares[y][x]->bit();
                if (bit) {
                    state += std::to_string(bit->gameTag());
                } else {
                    state += '0';
                }
            }
        }
    }

    return state;
}

void Grid::setStateString(const std::string& state)
{
    size_t index = 0;

    for (int y = 0; y < _height && index < state.length(); y++) {
        for (int x = 0; x < _width && index < state.length(); x++) {
            if (_enabled[y][x]) {
                char pieceChar = state[index++];

                // Clear existing piece
                _squares[y][x]->destroyBit();

                // This method just sets the state - games need to create their own pieces
                // when loading from state string based on the piece type
            }
        }
    }
}