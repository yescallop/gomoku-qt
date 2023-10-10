#pragma once

#include "common.hpp"

/// A stone on the board.
enum struct Stone { None = 0, Black = 1, White = 2 };

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
    std::unique_ptr<Stone[]> mat;

  public:
    RawBoard() : mat(new Stone[BOARD_SIZE * BOARD_SIZE]()) {}

    /// Checks if the board contains a point.
    bool contains_point(Point p) const {
        return p.x < BOARD_SIZE && p.y < BOARD_SIZE;
    }

    /// Returns a reference to the stone at the point.
    Stone &at(Point p) const {
        if (p.x >= BOARD_SIZE || p.y >= BOARD_SIZE)
            throw std::out_of_range("point out of board");
        return mat[p.y * BOARD_SIZE + p.x];
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
    size_t index;
    Point pos;
    Axis axis;
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

    /// Sets the stone at a point.
    bool set(Point p, Stone stone) {
        Stone &val = board.at(p);
        if (val != Stone::None)
            return false;
        val = stone;

        moves.resize(idx);
        moves.push_back({p, stone});
        idx += 1;

        if (!win || win->index >= idx) {
            auto res = check_win(p);
            if (res)
                win = {idx, p, res->first, res->second};
            else
                win = nullopt;
        }
        return true;
    }

    /// Unsets the last stone (if any).
    bool unset() {
        if (idx == 0)
            return false;
        idx -= 1;
        auto last = moves[idx];
        board.unset(last.pos);
        return true;
    }

    /// Resets the next stone (if any).
    bool reset() {
        if (idx >= total())
            return false;
        auto next = moves[idx];
        idx += 1;
        board.set(next.pos, next.stone);
        return true;
    }

    /// Jumps to the given index by unsetting or resetting stones.
    bool jump(usize to_index) {
        if (to_index > total())
            throw std::out_of_range("index out of range");
        if (idx == to_index)
            return false;
        if (idx < to_index) {
            for (usize i = idx; i < to_index; i++) {
                auto next = moves[i];
                board.set(next.pos, next.stone);
            }
        } else {
            for (usize i = idx; i > to_index; i--) {
                auto last = moves[i - 1];
                board.unset(last.pos);
            }
        }
        idx = to_index;
        return true;
    }

    /// Infers the next stone to play.
    Stone infer_turn() const {
        return idx == 0 ? Stone::Black : opposite(moves[idx - 1].stone);
    }

    /// Checks if there is a win at the point.
    optional<pair<Axis, Row>> check_win(Point p) const {
        Stone stone = board.at(p);
        if (stone == Stone::None)
            return nullopt;

        Row row;
        for (Axis axis : AXES) {
            if (scan_row(p, axis, row) >= 5)
                return pair(axis, row);
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
