#pragma once

#include "common.hpp"

/// A stone on the board.
enum struct Stone : u8 { None = 0, Black = 1, White = 2 };

/// Returns the opposite stone.
constexpr Stone opposite(Stone stone) {
    if (stone == Stone::None)
        return stone;
    return Stone(int(stone) ^ 3);
}

static_assert(opposite(Stone::Black) == Stone::White);
static_assert(opposite(Stone::White) == Stone::Black);

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

const usize BOARD_SIZE = 15;

// Inspired by `RawVec` in Rust.
class RawBoard {
    vector<Stone> mat{BOARD_SIZE * BOARD_SIZE};

  public:
    /// Checks if the board contains a point.
    bool contains_point(Point p) const {
        return p.x < BOARD_SIZE && p.y < BOARD_SIZE;
    }

    /// Returns a reference to the stone at the point.
    Stone &at(Point p) {
        if (!contains_point(p))
            throw std::out_of_range("point out of board");
        return mat.at(p.y * BOARD_SIZE + p.x);
    }

    /// Returns a const reference to the stone at the point.
    const Stone &at(Point p) const {
        if (!contains_point(p))
            throw std::out_of_range("point out of board");
        return mat.at(p.y * BOARD_SIZE + p.x);
    }

    /// Sets the stone at a point.
    void set(Point p, Stone stone) { at(p) = stone; }

    /// Unsets the stone at a point.
    void unset(Point p) { at(p) = Stone::None; }
};

struct Move {
    Point pos;
    Stone stone;
};

struct Row {
    Point start;
    Point end;
};

struct Win {
    usize index;
    Row row;
};

class Board {
    RawBoard board;
    vector<Move> moves;
    usize idx = 0;
    optional<Win> win;

  public:
    /// Returns the total number of moves, on or off the board,
    /// in the past or in the future.
    usize total() const { return moves.size(); }

    /// Returns the current move index.
    usize index() const { return idx; }

    /// Returns a span of moves in the past.
    span<const Move> past_moves() const { return {moves.data(), idx}; }

    /// Returns the first win witnessed in the past (if any).
    optional<Win> first_win() const {
        if (win && win->index <= idx)
            return win;
        else
            return nullopt;
    }

    /// Gets the stone at a point.
    Stone get(Point p) const { return board.at(p); }

    /// Sets the stone at a point, clearing moves in the future.
    bool set(Point p, Stone stone) {
        Stone &val = board.at(p);
        if (val != Stone::None)
            return false;
        val = stone;

        moves.resize(idx);
        moves.push_back({p, stone});
        idx += 1;

        if (!win || win->index >= idx) {
            auto win_row = find_win_row(p);
            if (win_row)
                win = {idx, *win_row};
            else
                win = nullopt;
        }
        return true;
    }

    /// Unsets the last move (if any).
    bool unset() {
        if (idx == 0)
            return false;
        idx -= 1;
        Move last = moves.at(idx);
        board.unset(last.pos);
        return true;
    }

    /// Resets the next move (if any).
    bool reset() {
        if (idx >= total())
            return false;
        Move next = moves.at(idx);
        idx += 1;
        board.set(next.pos, next.stone);
        return true;
    }

    /// Jumps to the given index by unsetting or resetting moves.
    bool jump(usize to_index) {
        if (to_index > total())
            throw std::out_of_range("index out of range");
        if (idx == to_index)
            return false;
        if (idx < to_index) {
            for (usize i = idx; i < to_index; i++) {
                Move next = moves.at(i);
                board.set(next.pos, next.stone);
            }
        } else {
            for (usize i = idx; i > to_index; i--) {
                Move last = moves.at(i - 1);
                board.unset(last.pos);
            }
        }
        idx = to_index;
        return true;
    }

    /// Infers the next stone to play.
    Stone infer_turn() const {
        return idx == 0 ? Stone::Black : opposite(moves.at(idx - 1).stone);
    }

    /// Searches for a win row through the point.
    optional<Row> find_win_row(Point p) const {
        Stone stone = board.at(p);
        if (stone == Stone::None)
            return nullopt;

        Row row;
        for (Axis axis : AXES) {
            if (scan_row(p, axis, row) >= 5)
                return row;
        }
        return nullopt;
    }

    /// Scans the row at a point in the direction of the axis.
    u32 scan_row(Point p, Axis axis, Row &row) const {
        Stone stone = board.at(p);
        u32 len = 1;

        auto scan = [&](Point &cur, bool forward) {
            Point next;
            while (board.contains_point(next = cur.adjacent(axis, forward)) &&
                   board.at(next) == stone) {
                len += 1;
                cur = next;
            }
        };

        row = {p, p};
        scan(row.start, false);
        scan(row.end, true);
        return len;
    }
};
