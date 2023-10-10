#pragma once

#include "binary.hpp"
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

bool in_board(Point p) { return p.x < BOARD_SIZE && p.y < BOARD_SIZE; }

// Inspired by `RawVec` in Rust.
class RawBoard {
    vector<Stone> mat{BOARD_SIZE * BOARD_SIZE};

  public:
    /// Returns a reference to the stone at the point.
    Stone &at(Point p) {
        if (!in_board(p))
            throw std::out_of_range("point out of board");
        return mat.at(p.y * BOARD_SIZE + p.x);
    }

    /// Returns a const reference to the stone at the point.
    const Stone &at(Point p) const {
        if (!in_board(p))
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
            if (auto win_row = find_win_row(p))
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
            while (in_board(next = cur.adjacent(axis, forward)) &&
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

    /// Serializes the board into a byte array.
    QByteArray serialize() const {
        QByteArray buf;
        for (auto [pos, stone] : past_moves()) {
            i32 x = i32(pos.x) - BOARD_SIZE / 2;
            i32 y = i32(pos.y) - BOARD_SIZE / 2;
            u32 index = interleave(zigzag_encode(x), zigzag_encode(y));
            write_var_u14(buf, (index << 1) | (u32(stone) - 1));
        }
        return buf;
    }

    /// Deserializes the byte array into a board.
    static optional<Board> deserialize(const QByteArray &buf) {
        Board board;
        usize read = 0;

        while (read < buf.size()) {
            auto val = read_var_u14(buf, read);
            if (!val)
                return nullopt;

            Stone stone = Stone((*val & 1) + 1);
            auto [ux, uy] = deinterleave(*val >> 1);

            i32 x = zigzag_decode(ux) + BOARD_SIZE / 2;
            i32 y = zigzag_decode(uy) + BOARD_SIZE / 2;
            Point pos(x, y);
            if (!in_board(pos) || !board.set(pos, stone))
                return nullopt;
        }
        return board;
    }
};
