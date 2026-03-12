#pragma once

#include "ChessSquare.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>

class Grid
{
public:
    Grid(int width, int height);
    ~Grid();

    // Basic access
    ChessSquare* getSquare(int x, int y);
    ChessSquare* getSquareByIndex(int index);
    bool isValid(int x, int y) const;
    bool isEnabled(int x, int y) const;
    void setEnabled(int x, int y, bool enabled);

    // Grid properties
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getIndex(int x, int y) const { return y * _width + x; }
    void getCoordinates(int index, int& x, int& y) const;

    // Directional helpers (built into Grid)
    ChessSquare* getFL(int x, int y);  // front-left (up-left diagonal)
    ChessSquare* getFR(int x, int y);  // front-right (up-right diagonal)
    ChessSquare* getBL(int x, int y);  // back-left (down-left diagonal)
    ChessSquare* getBR(int x, int y);  // back-right (down-right diagonal)

    // Orthogonal directions
    ChessSquare* getN(int x, int y);   // north (up)
    ChessSquare* getS(int x, int y);   // south (down)
    ChessSquare* getE(int x, int y);   // east (right)
    ChessSquare* getW(int x, int y);   // west (left)

    // Double-distance helpers
    ChessSquare* getFLFL(int x, int y) { auto s = getFL(x, y); return s ? getFL(s->getColumn(), s->getRow()) : nullptr; }
    ChessSquare* getFRFR(int x, int y) { auto s = getFR(x, y); return s ? getFR(s->getColumn(), s->getRow()) : nullptr; }
    ChessSquare* getBLBL(int x, int y) { auto s = getBL(x, y); return s ? getBL(s->getColumn(), s->getRow()) : nullptr; }
    ChessSquare* getBRBR(int x, int y) { auto s = getBR(x, y); return s ? getBR(s->getColumn(), s->getRow()) : nullptr; }

    // Graph connections (for Hitman Go style games)
    void addConnection(int fromIndex, int toIndex);
    void addConnection(int fromX, int fromY, int toX, int toY);
    std::vector<ChessSquare*> getConnectedSquares(int x, int y);
    bool areConnected(int fromX, int fromY, int toX, int toY);

    // Iterator support
    void forEachSquare(std::function<void(ChessSquare*, int x, int y)> func);
    void forEachEnabledSquare(std::function<void(ChessSquare*, int x, int y)> func);

    // Initialize squares with positions and sprites
    void initializeChessSquares(float squareSize, const char* spriteName);
    void initializeSquares(float squareSize, const char* spriteName);
    void initializeSquare(int x, int y, float squareSize, const char* spriteName);

    // State management (for enabled squares only)
    std::string getStateString() const;
    void setStateString(const std::string& state);

private:
    std::vector<std::vector<ChessSquare*>> _squares;
    std::vector<std::vector<bool>> _enabled;
    std::unordered_map<int, std::vector<int>> _connections;
    int _width;
    int _height;
};