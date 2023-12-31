#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QByteArray>

typedef std::uint8_t u8;
typedef std::int32_t i32;
typedef std::uint32_t u32;
typedef std::size_t usize;

using std::nullopt;
using std::optional;
using std::pair;
using std::span;
using std::vector;

/// A stone on the board.
enum struct Stone : u8 { None = 0, Black = 1, White = 2 };

/// Returns the opposite stone.
constexpr Stone opposite(Stone stone) {
    switch (stone) {
    case Stone::Black:
        return Stone::White;
    case Stone::White:
        return Stone::Black;
    }
    return Stone::None;
}

/// Axes on the board.
enum struct Axis { Vertical, Ascending, Horizontal, Descending };

const Axis AXES[] = {Axis::Vertical, Axis::Ascending, Axis::Horizontal,
                     Axis::Descending};

/// Returns the unit vector in the direction of the axis.
pair<i32, i32> unit_vec(Axis axis) {
    switch (axis) {
    case Axis::Vertical:
        return {0, 1};
    case Axis::Ascending:
        return {1, -1};
    case Axis::Horizontal:
        return {1, 0};
    case Axis::Descending:
        return {1, 1};
    }
    return {0, 0};
}

/// A 2D point with `uint32_t` coordinates.
struct Point {
    u32 x;
    u32 y;

    Point() : x(0), y(0) {}
    Point(u32 x, u32 y) : x(x), y(y) {}

    bool operator==(Point other) const { return x == other.x && y == other.y; }

    /// Returns the adjacent point in the direction of the axis.
    Point adjacent(Axis axis, bool forward) const {
        auto [dx, dy] = unit_vec(axis);
        return forward ? Point(x + dx, y + dy) : Point(x - dx, y - dy);
    }
};

/// A contiguous row of stones on the board.
struct Row {
    Point start;
    Point end;
};

const usize BOARD_SIZE = 15;

/// Checks if a point is within the board boundary.
bool in_board(Point p) { return p.x < BOARD_SIZE && p.y < BOARD_SIZE; }

/// A 15x15 gomoku board.
class Board {
    vector<Stone> mat{BOARD_SIZE * BOARD_SIZE};

  public:
    /// Returns a reference to the stone at a point.
    Stone &at(Point p) {
        if (!in_board(p))
            throw std::out_of_range("point out of board");
        return mat.at(p.y * BOARD_SIZE + p.x);
    }

    /// Returns a const reference to the stone at a point.
    const Stone &at(Point p) const {
        if (!in_board(p))
            throw std::out_of_range("point out of board");
        return mat.at(p.y * BOARD_SIZE + p.x);
    }

    /// Sets the stone at a point.
    void set(Point p, Stone stone) { at(p) = stone; }

    /// Unsets the stone at a point.
    void unset(Point p) { at(p) = Stone::None; }

    /// Scans the row through a point in the direction of the axis.
    u32 scan_row(Point p, Axis axis, Row &row) const {
        Stone stone = at(p);
        u32 len = 1;

        auto scan = [&](Point &cur, bool forward) {
            Point next;
            while (in_board(next = cur.adjacent(axis, forward)) &&
                   at(next) == stone) {
                len += 1;
                cur = next;
            }
        };

        row = {p, p};
        scan(row.start, false);
        scan(row.end, true);
        return len;
    }

    /// Searches for a win row through the point.
    optional<Row> find_win_row(Point p) const {
        Stone stone = at(p);
        if (stone == Stone::None)
            return nullopt;

        Row row;
        for (Axis axis : AXES) {
            if (scan_row(p, axis, row) >= 5)
                return row;
        }
        return nullopt;
    }
};

/// A move on the board, namely a (position, stone) pair.
struct Move {
    Point pos;
    Stone stone;

    bool operator==(Move other) const {
        return pos == other.pos && stone == other.stone;
    }
};

/// A win witnessed on the board, namely a (move index, row) pair.
struct Win {
    usize index;
    Row row;
};

/// A gomoku game, namely a record of moves.
class Game {
    Board board;
    vector<Move> moves;
    usize index = 0;
    optional<Win> win;

    /// Control bytes used in serialization.
    enum CtrlByte : u8 { BEGIN_SEQUENCE = 0xff, END_SEQUENCE = 0xfe };

    // Ensure that position bytes cannot collide with control bytes
    // in serialization.
    static_assert(BOARD_SIZE * BOARD_SIZE < 0xfe);

  public:
    bool operator==(const Game &other) const {
        return moves == other.moves && index == other.index;
    }

    /// Returns the total number of moves, on or off the board,
    /// in the past or in the future.
    usize total_moves() const { return moves.size(); }

    /// Returns the current move index.
    usize move_index() const { return index; }

    /// Returns a span of moves in the past.
    span<const Move> past_moves() const { return {moves.data(), index}; }

    /// Returns a span of moves in the future.
    span<const Move> future_moves() const {
        return {moves.data() + index, moves.size() - index};
    }

    /// Returns the first win witnessed in the past (if any).
    optional<Win> first_win() const {
        if (win && win->index <= index)
            return win;
        else
            return nullopt;
    }

    /// Gets the stone at a point.
    Stone stone_at(Point p) const { return board.at(p); }

    /// Makes a move at a point, clearing moves in the future.
    bool make_move(Point p, Stone stone) {
        Stone &val = board.at(p);
        if (val != Stone::None)
            return false;
        val = stone;

        moves.resize(index);
        moves.push_back({p, stone});
        index += 1;

        if (!win || win->index >= index) {
            if (auto win_row = board.find_win_row(p))
                win = {index, *win_row};
            else
                win = nullopt;
        }
        return true;
    }

    /// Undoes the last move (if any).
    bool undo() {
        if (index == 0)
            return false;
        index -= 1;
        Move last = moves.at(index);
        board.unset(last.pos);
        return true;
    }

    /// Redoes the next move (if any).
    bool redo() {
        if (index >= moves.size())
            return false;
        Move next = moves.at(index);
        index += 1;
        board.set(next.pos, next.stone);
        return true;
    }

    /// Jumps to the given move index by undoing or redoing moves.
    bool jump(usize to_index) {
        if (to_index > moves.size())
            throw std::out_of_range("move index out of range");
        if (index == to_index)
            return false;
        if (index > to_index) {
            for (usize i = index; i > to_index; i--) {
                Move last = moves.at(i - 1);
                board.unset(last.pos);
            }
        } else {
            for (usize i = index; i < to_index; i++) {
                Move next = moves.at(i);
                board.set(next.pos, next.stone);
            }
        }
        index = to_index;
        return true;
    }

    /// Infers the next stone to play, based on past moves.
    Stone infer_turn() const {
        return index == 0 ? Stone::Black : opposite(moves.at(index - 1).stone);
    }

    /// Serializes the game into a byte array.
    QByteArray serialize() const {
        QByteArray buf;

        auto moves = past_moves();
        if (!moves.empty() && moves[0].stone == Stone::White) {
            buf.append(CtrlByte::BEGIN_SEQUENCE);
            buf.append(CtrlByte::END_SEQUENCE);
        }

        Stone last_stone = Stone::None;
        bool in_sequence = false;
        for (auto [pos, stone] : moves) {
            if (last_stone == stone) {
                if (!in_sequence) {
                    buf.insert(buf.size() - 1, CtrlByte::BEGIN_SEQUENCE);
                    in_sequence = true;
                }
            } else if (in_sequence) {
                buf.append(CtrlByte::END_SEQUENCE);
                in_sequence = false;
            }
            buf.append(pos.y * BOARD_SIZE + pos.x);
            last_stone = stone;
        }

        if (in_sequence)
            buf.append(CtrlByte::END_SEQUENCE);
        return buf;
    }

    /// Deserializes the byte array into a game.
    static optional<Game> deserialize(const QByteArray &buf) {
        Game game;
        Stone stone = Stone::Black;
        bool in_sequence = false;

        for (u8 byte : buf) {
            if (byte == CtrlByte::BEGIN_SEQUENCE) {
                if (in_sequence)
                    return nullopt;
                in_sequence = true;
                continue;
            }
            if (byte == CtrlByte::END_SEQUENCE) {
                if (!in_sequence)
                    return nullopt;
                in_sequence = false;
                stone = opposite(stone);
                continue;
            }

            Point pos(byte % BOARD_SIZE, byte / BOARD_SIZE);
            if (!in_board(pos) || !game.make_move(pos, stone))
                return nullopt;
            if (!in_sequence)
                stone = opposite(stone);
        }

        if (in_sequence)
            return nullopt;
        return game;
    }
};
