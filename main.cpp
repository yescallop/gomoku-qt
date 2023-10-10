#include <QtWidgets>

#include "gomoku_core.hpp"

const int WINDOW_SIZE = 600;
const QColor BOARD_BACKGROUND_COLOR(0xffcc66);

const double TENTATIVE_MOVE_OPACITY = 0.5;

const double BORDER_WIDTH_RATIO = 12.0;
const double LINE_WIDTH_RATIO = 24.0;
const double WIN_HINT_WIDTH_RATIO = 12.0;
const double STAR_RADIUS_RATIO = 10.0;
const double STONE_RADIUS_RATIO = 2.25;

const Point STAR_POSITIONS[] = {{3, 3}, {3, 11}, {7, 7}, {11, 3}, {11, 11}};

class BoardWidget : public QWidget {
    Board board;
    Stone stone = Stone::Black;
    optional<Point> tentative_move;
    bool view_mode_enabled = false;
    bool win_hint_enabled = false;

    double grid_size;

    QAction *pass_act;
    QAction *undo_act;
    QAction *redo_act;
    QAction *clear_act;
    QAction *view_mode_act;
    QAction *win_hint_act;

  public:
    BoardWidget() {
        pass_act = new QAction("让子", this);
        undo_act = new QAction("悔棋", this);
        redo_act = new QAction("复位", this);
        clear_act = new QAction("清空", this);
        view_mode_act = new QAction("查看模式", this);
        view_mode_act->setCheckable(true);
        win_hint_act = new QAction("胜利提示", this);
        win_hint_act->setCheckable(true);

        connect(pass_act, &QAction::triggered, this, &BoardWidget::pass);
        connect(undo_act, &QAction::triggered, this, &BoardWidget::undo);
        connect(redo_act, &QAction::triggered, this, &BoardWidget::redo);
        connect(clear_act, &QAction::triggered, this, &BoardWidget::clear);
        connect(view_mode_act, &QAction::triggered, this,
                &BoardWidget::view_mode);
        connect(win_hint_act, &QAction::triggered, this,
                &BoardWidget::win_hint);
    }

    // Helper methods.
  private:
    optional<Point> to_board_pos(QPointF pos) const {
        double x = pos.x() / grid_size - 0.5;
        double y = pos.y() / grid_size - 0.5;

        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE)
            return nullopt;
        return Point(x, y);
    }

    QPointF to_widget_pos(Point pos) const {
        return {(pos.x + 1) * grid_size, (pos.y + 1) * grid_size};
    }

    void draw_circle(QPainter &p, Point pos, double radius) const {
        p.drawEllipse(to_widget_pos(pos), radius, radius);
    }

    void stone_updated() {
        repaint();

        usize index = board.index(), total = board.total();
        QString index_str =
            index == 0 ? QString("开局") : QString("第 %1 手").arg(index);
        QString title;
        if (index == total) {
            title = QString("五子棋 (%1)").arg(index_str);
        } else {
            title = QString("五子棋 (%1 / 共 %2 手)").arg(index_str).arg(total);
        }
        ((QMainWindow *)parent())->setWindowTitle(title);
    }

    // Event handlers.
  protected:
    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu(this);
        menu.addActions({pass_act, undo_act, redo_act, clear_act});
        menu.addSeparator();
        menu.addActions({view_mode_act, win_hint_act});
        menu.exec(event->globalPos());
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Draw the board background.
        p.setPen(Qt::NoPen);
        p.setBrush(BOARD_BACKGROUND_COLOR);
        p.drawRect(rect());

        // Draw the lines and the border.
        int w = width();
        grid_size = double(w) / (BOARD_SIZE + 1);

        double border_width = grid_size / BORDER_WIDTH_RATIO;
        double line_width = grid_size / LINE_WIDTH_RATIO;

        for (int i = 1; i <= BOARD_SIZE; i++) {
            double pos = grid_size * i;

            if (i == 1 || i == BOARD_SIZE)
                p.setPen(QPen(Qt::black, border_width));
            else
                p.setPen(QPen(Qt::black, line_width));

            p.drawLine(QPointF(grid_size, pos), QPointF(w - grid_size, pos));
            p.drawLine(QPointF(pos, grid_size), QPointF(pos, w - grid_size));
        }

        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);

        // Draw the stars.
        double star_radius = grid_size / STAR_RADIUS_RATIO;
        for (Point pos : STAR_POSITIONS) {
            draw_circle(p, pos, star_radius);
        }

        // Draw the stones.
        double stone_radius = grid_size / STONE_RADIUS_RATIO;
        auto moves = board.past_moves();
        for (auto [pos, val] : moves) {
            p.setBrush(val == Stone::Black ? Qt::black : Qt::white);
            draw_circle(p, pos, stone_radius);
        }

        // Draw the win hint.
        auto win = board.first_win();
        if (win_hint_enabled && win) {
            double win_hint_width = grid_size / WIN_HINT_WIDTH_RATIO;
            p.setPen(QPen(Qt::red, win_hint_width, Qt::DotLine));

            auto [start, end] = win->row;
            p.drawLine(to_widget_pos(start), to_widget_pos(end));
        }

        p.setPen(Qt::NoPen);

        // Draw a star on the last stone placed.
        if (!moves.empty()) {
            auto [pos, val] = moves.back();
            p.setBrush(val == Stone::Black ? Qt::white : Qt::black);
            draw_circle(p, pos, star_radius);
        }

        // Draw the tentative move.
        if (!view_mode_enabled && tentative_move) {
            p.setBrush(stone == Stone::Black ? Qt::black : Qt::white);
            p.setOpacity(TENTATIVE_MOVE_OPACITY);
            draw_circle(p, *tentative_move, stone_radius);
        }
    }

    void hoverEvent(QSinglePointEvent *event) {
        if (view_mode_enabled)
            return;
        optional<Point> p = to_board_pos(event->position());
        if (p && board.get(*p) != Stone::None)
            p = nullopt;
        if (tentative_move != p) {
            tentative_move = p;
            repaint();
        }
    }

    void enterEvent(QEnterEvent *event) override { hoverEvent(event); }
    void mouseMoveEvent(QMouseEvent *event) override { hoverEvent(event); }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton || view_mode_enabled)
            return;
        auto p = to_board_pos(event->position());
        if (!p || !board.set(*p, stone))
            return;

        stone = opposite(stone);
        tentative_move = nullopt;
        stone_updated();
    }

    void wheelEvent(QWheelEvent *event) override {
        if (!view_mode_enabled)
            return;
        bool forward = event->angleDelta().y() > 0;
        if (forward ? board.reset() : board.unset())
            stone_updated();
    }

    // Menu slots.
  private:
    void pass() {
        stone = opposite(stone);
        if (tentative_move)
            repaint();
    }

    void undo() {
        if (board.unset()) {
            stone = board.infer_turn();
            stone_updated();
        }
    }

    void redo() {
        if (board.reset()) {
            stone = board.infer_turn();
            stone_updated();
        }
    }

    void clear() {
        if (board.jump(0)) {
            stone = board.infer_turn();
            stone_updated();
        }
    }

    void view_mode(bool enabled) {
        view_mode_enabled = enabled;
        if (!enabled) {
            stone = board.infer_turn();
        } else if (tentative_move) {
            tentative_move = nullopt;
            repaint();
        }
    }

    void win_hint(bool enabled) {
        win_hint_enabled = enabled;
        if (board.first_win())
            repaint();
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    BoardWidget *widget = new BoardWidget();
    widget->setMouseTracking(true);

    QMainWindow window;
    window.setCentralWidget(widget);
    window.setFixedSize(WINDOW_SIZE, WINDOW_SIZE);
    window.setWindowTitle("五子棋 (开局)");
    window.show();

    return app.exec();
}
