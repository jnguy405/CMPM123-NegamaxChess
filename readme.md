## CMPM123 Negamax Chess (Bitboards + ImGui)

This project is a chess game with an AI based on Negamax + alpha-beta pruning. The engine uses bitboards for fast move generation and evaluation, and uses ImGui for the UI (board display, AI controls, logging, and a small in-app command line).

---

### Key Files

- `classes/Chess.h` / `classes/Chess.cpp`: chess engine, move generation, legality checks, and AI search.
- `Application.cpp`: ImGui UI wiring (search depth slider, “Force AI Move”, game end checks, and command input).
- `Logger.h` + `Command.h`/`Command.cpp`: logging macros and the simple command system shown in the UI.
- `classes/MagicBitboards.h`: sliding-piece attack generation (rook/bishop) using magic bitboards.

---

### UI Flow (ImGui)

The UI owns the game loop interaction:

1. The “Search Depth” slider sets `chessAIDepth` using:
   - `ImGui::SliderInt("Search Depth", &chessAIDepth, 1, 5, "Depth %d");` in `Application.cpp`
2. When rendering and it is an AI turn, `game->updateAI()` is called.
3. After each move, `EndOfTurn()` checks the outcome:
   - `Player *winner = game->checkForWinner();`
   - if no winner, `game->checkForDraw()` determines draw conditions.
4. A command line is displayed using:
   - `ImGui::InputText("##CommandInput", InputBuf, ..., &Command::TextEditCallbackStub)`
   - On Enter, the code calls `Command::ExecCommand(s)`.

---

### Logging and Commands

Logging is implemented with a singleton `ClassGame::Logger` and macros:

- `LOG_INFO(msg)` / `LOG_INFO_TAG(msg, tag)`
- `LOG_WARN(msg)` / `LOG_ERROR(msg)`

Command entry is routed through `Application.cpp` -> `Command::ExecCommand()`:

- `Command::ExecCommand(const char* command_line)` recognizes (at least) `CLEAR, HELP, INFO, WARN, ERROR, RESET`.
- These commands primarily clear or write to the logger.

The logger is also rendered with ImGui in the “Game Log” window:

- `Logger::GetInstance().GetEntries()` is iterated, filtered (by `[INFO]`, `[WARN]`, `[ERROR]`, `[AI SCORE]`, `[DEBUG]` tags), and drawn with `ImGui::Text(...)`.
- The command input uses `ImGui::InputText(...)` and calls `Command::ExecCommand(s)` when Enter is pressed.

---

## Engine Representation: State String + Bitboards

#### `stateString()` (64 squares)

The engine can serialize the board to a 64-character string:

- `Chess::stateString()` loops over `y=0..7` and `x=0..7` and concatenates `pieceNotation(x, y)`.
- `pieceNotation(x, y)` uses:
  - uppercase for White: `P N B R Q K`
  - lowercase for Black: `p n b r q k`
  - `'0'` for empty squares.

This same string format is used for search nodes (so the recursion can evaluate hypothetical move states without touching the UI board).

---

#### Bitboards

Internally, the engine maintains per-piece/per-color bitboards:

- `_whitePawns, _whiteKnights, _whiteBishops, _whiteRooks, _whiteQueens, _whiteKing`
- `_blackPawns, _blackKnights, _blackBishops, _blackRooks, _blackQueens, _blackKing`

Key synchronization helpers:

- `stateStringToBitboards(state)` converts the 64-char string into those bitboards.
- `updateBitboardsFromGrid()` / `updateGridFromBitboards()` keep UI and engine in sync after actual moves.

---

### Move Representation: `BitMove`

Moves are represented by `BitMove` (see `classes/Bitboard.h`) with:

- `from`, `to` (0..63)
- `piece` (moving piece type)
- `promotion` (stores the promoted piece type when relevant)
- `flags` (special move flags such as `MoveCapture`, `MoveEnPassant`, `MoveCastleKingSide`, `MoveCastleQueenSide`, `MovePromotion`)

The AI and legality generator depend on these flags to handle special rules correctly.

---

### Legal Move Generation

The engine generates moves in two stages:

1. **Pseudo-legal move generation** from bitboards.
2. **King-safety filtering** when `legalOnly == true`.

---

#### How legality is enforced

`Chess::generateAllMovesFromBitboards(state, currentColor, legalOnly, ...)`:

- Builds a list of pseudo moves by calling:
  - `generatePawnMoves(pseudoMoves, currentColor)`
  - `generatePieceMoves(... Knight ...)`
  - `generatePieceMoves(... Bishop ...)`
  - `generatePieceMoves(... Rook ...)`
  - `generatePieceMoves(... Queen ...)`
  - `generatePieceMoves(... King ...)`
  - `generateCastlingMoves(pseudoMoves, currentColor)`

- If `legalOnly` is enabled, each candidate move is applied to the *string state*:
  - `nextState = applyMoveToState(state, move, currentColor)`
  - `stateStringToBitboards(nextState)`
  - then filtered by:
    - `!isKingInCheck(currentColor)`

---

#### Attack detection

King safety uses:

- `isKingInCheck(color)` -> `isSquareAttacked(kingSq, oppositeColor)`
- `isSquareAttacked(...)` checks:
  - pawn attackers (directional shifts)
  - knight/king (precomputed attack tables)
  - bishops/queens and rooks/queens via sliding attacks:
    - rook/bishop attacks come from `MagicBitboards.h` (`getRookAttacks`, `getBishopAttacks`)

---

### Special Moves

- En passant
  - `_enPassantSquare` is set when a pawn advances two squares and is cleared after other moves.
  - `generatePawnMoves()` adds en-passant captures when `_enPassantSquare` is valid.
- Castling
  - Castling rights are tracked with `_whiteKingMoved`, `_whiteRookAFileMoved`, `_whiteRookHFileMoved` (and black equivalents).
  - `generateCastlingMoves()` checks:
    - king/rook unmoved
    - empty squares between king and rook
    - squares not attacked by the opponent
- Promotions
  - pawn promotion moves are generated with `MovePromotion` and the promotion piece is currently treated as a queen in `applyMoveToState()`.

---

## AI Search (Negamax + Alpha-Beta)

#### Root entry: `Chess::updateAI()`

The AI:

- reads the current board `stateString()`
- uses the current engine depth from `_searchDepth` (set via `Chess::setSearchDepth(int depth)`)
- generates legal moves for the AI side:
  - `generateAllMovesFromBitboards(state, aiColor, true)`
- evaluates each candidate by calling Negamax on a hypothetical state.

---

#### Core: `Chess::negamax()`

`negamax(state, depth, alpha, beta, currentColor, ...)`:

- terminal test:
  - `aiTestForTerminalState(state, currentColor, winner)` checks king capture and “no legal moves” (checkmate vs stalemate)
- if depth == 0 (or terminal), it calls `evaluateBoard(state)`
- move ordering:
  - captures are ordered with an MVV-LVA style heuristic
  - non-captures use killer moves + history heuristic
  - it sorts moves before searching them (improves alpha-beta pruning)

---

#### Search metadata snapshot/restore

Because the legality generator depends on stateful flags (castling rights, en-passant square), Negamax snapshots and restores search state between candidate moves using:

- `NodeSearchState captureNodeSearchState() const;`
- `restoreNodeSearchState(const NodeSearchState& s);`

This prevents recursive move searches from corrupting the metadata for sibling branches.

---

### Draw Rules (50-move + Threefold Repetition)

The engine keeps:

- `_halfMoveClock` for the 50-move rule
  - it resets on pawn moves and captures
  - it increments otherwise
- `_repetitionCounts` keyed by `repetitionKey()` for threefold repetition
  - the key incorporates side-to-move, castling rights, and en-passant square.

`checkForDraw()` is invoked by `Application.cpp`’s `EndOfTurn()` after `checkForWinner()`.

---

### Bitboards + Magic Bitboards (`MagicBitboards.h`)

Sliding pieces (rook/bishop/queen) use magic bitboards:

- `initMagicBitboards()` allocates rook and bishop attack tables.
- `getRookAttacks(square, occupied)` and `getBishopAttacks(square, occupied)` perform:
  - mask blockers
  - multiply by magic number
  - shift to index
  - read from precomputed `RAttacks` / `BAttacks`.

`Chess.cpp` ensures magic tables are initialized via a static initializer:

- `MagicInit()` calls `initMagicBitboards()`.

---

### Build/Run (Debug)

This repository uses CMake. Typical workflow:

- build `demo.exe` in `build/Debug`
- run it, then use ImGui:
  - “Start Chess” to begin
  - “Search Depth” to set AI depth
  - “Force AI Move” to trigger AI immediately (useful for debugging)
  - the command input for quick logger tests / shortcuts