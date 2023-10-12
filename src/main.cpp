#include <QtWidgets>

#include "core.hpp"

const int WINDOW_SIZE = 600;
const QColor BOARD_BACKGROUND_COLOR(0xffcc66);

const double TENTATIVE_MOVE_OPACITY = 0.5;

const double BORDER_WIDTH_RATIO = 12.0;
const double LINE_WIDTH_RATIO = 24.0;
const double WIN_HINT_WIDTH_RATIO = 12.0;
const double STAR_RADIUS_RATIO = 10.0;
const double STONE_RADIUS_RATIO = 2.25;
const double ORDINAL_SIZE_RATIOS[] = {0.65, 0.75, 0.85};

const Point STAR_POSITIONS[] = {{3, 3}, {3, 11}, {7, 7}, {11, 3}, {11, 11}};

const QByteArray URI_PREFIX("gomoku://");

bool confirm(QWidget *parent, const QString &consequence) {
    QMessageBox box(parent);
    box.setWindowTitle("确认操作");
    box.setIcon(QMessageBox::Question);
    box.setText(QString("这将%1，是否继续操作？").arg(consequence));

    QPushButton *yes_button = box.addButton("是", QMessageBox::YesRole);
    QPushButton *no_button = box.addButton("否", QMessageBox::NoRole);
    box.setDefaultButton(no_button);
    box.exec();

    bool ret = box.clickedButton() == yes_button;
    delete yes_button;
    delete no_button;
    return ret;
}

class BoardWidget : public QWidget {
    Game game;
    Stone stone = Stone::Black;
    optional<Point> cursor_pos;

    double grid_size;

    QAction *pass_act;
    QAction *undo_act;
    QAction *redo_act;
    QAction *home_act;
    QAction *end_act;

    QAction *review_act;
    QAction *win_hint_act;
    QAction *ordinals_act;
    QAction *lock_stone_act;

    QAction *export_act;
    QAction *import_act;

    bool reviewing() const { return review_act->isChecked(); }
    bool shows_win_hint() const { return win_hint_act->isChecked(); }
    bool shows_ordinals() const { return ordinals_act->isChecked(); }
    bool stone_locked() const { return lock_stone_act->isChecked(); }

  public:
    BoardWidget() {
        pass_act = new QAction("让子", this);
        pass_act->setShortcut(Qt::CTRL | Qt::Key_P);
        undo_act = new QAction("悔棋", this);
        undo_act->setShortcut(Qt::CTRL | Qt::Key_Z);
        redo_act = new QAction("复位", this);
        redo_act->setShortcut(Qt::CTRL | Qt::Key_Y);
        home_act = new QAction("跳转至开局", this);
        home_act->setShortcut(Qt::Key_Home);
        end_act = new QAction("跳转至局末", this);
        end_act->setShortcut(Qt::Key_End);

        review_act = new QAction("复盘模式", this);
        review_act->setCheckable(true);
        win_hint_act = new QAction("胜利提示", this);
        win_hint_act->setCheckable(true);
        ordinals_act = new QAction("序号显示", this);
        ordinals_act->setCheckable(true);
        lock_stone_act = new QAction("锁定棋子", this);
        lock_stone_act->setCheckable(true);

        export_act = new QAction("导出至剪贴板", this);
        export_act->setShortcut(Qt::CTRL | Qt::Key_C);
        import_act = new QAction("自剪贴板导入", this);
        import_act->setShortcut(Qt::CTRL | Qt::Key_V);

        connect(pass_act, &QAction::triggered, this, &BoardWidget::pass);
        connect(undo_act, &QAction::triggered, this, &BoardWidget::undo);
        connect(redo_act, &QAction::triggered, this, &BoardWidget::redo);
        connect(home_act, &QAction::triggered, this, &BoardWidget::home);
        connect(end_act, &QAction::triggered, this, &BoardWidget::end);

        connect(review_act, &QAction::toggled, this,
                &BoardWidget::toggle_review);
        connect(win_hint_act, &QAction::toggled, this,
                &BoardWidget::toggle_win_hint);
        connect(ordinals_act, &QAction::toggled, this,
                &BoardWidget::toggle_ordinals);

        connect(export_act, &QAction::triggered, this,
                &BoardWidget::export_game);
        connect(import_act, &QAction::triggered, this,
                &BoardWidget::import_game);

        // This is required for the shortcuts to work.
        addActions({pass_act, undo_act, redo_act, home_act, end_act, export_act,
                    import_act});
    }

    ~BoardWidget() {
        delete pass_act;
        delete undo_act;
        delete redo_act;
        delete home_act;
        delete end_act;

        delete review_act;
        delete win_hint_act;
        delete ordinals_act;
        delete lock_stone_act;

        delete export_act;
        delete import_act;
    }

    bool can_close_now() { return game.total_moves() == 0; }

    // Helper methods.
  private:
    optional<Point> to_game_pos(QPointF pos) const {
        double x = pos.x() / grid_size - 0.5;
        double y = pos.y() / grid_size - 0.5;

        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE)
            return nullopt;
        return Point(x, y);
    }

    QPointF to_screen_pos(Point pos) const {
        return {(pos.x + 1) * grid_size, (pos.y + 1) * grid_size};
    }

    void draw_circle(QPainter &p, Point pos, double radius) const {
        p.drawEllipse(to_screen_pos(pos), radius, radius);
    }

    void game_updated() {
        usize index = game.move_index(), total = game.total_moves();
        QString index_str =
            index == 0 ? QString("开局") : QString("第 %1 手").arg(index);
        QString title;
        if (index == total) {
            title = QString("五子棋 (%1)").arg(index_str);
        } else {
            title = QString("五子棋 (%1 / 共 %2 手)").arg(index_str).arg(total);
        }
        ((QMainWindow *)parent())->setWindowTitle(title);

        repaint();
    }

    void import_failed(const QString &text) {
        QMessageBox box(this);
        box.setWindowTitle("自剪贴板导入失败");
        box.setIcon(QMessageBox::Warning);
        box.setText(text);
        box.setInformativeText("请检查剪贴板中的文本是否有误。");
        QPushButton *ok_button = box.addButton("确定", QMessageBox::AcceptRole);
        box.exec();
        delete ok_button;
    }

    void infer_turn() {
        if (!stone_locked())
            stone = game.infer_turn();
    }

    optional<Point> filter_unoccupied(optional<Point> p) const {
        if (p && game.stone_at(*p) != Stone::None)
            p = nullopt;
        return p;
    }

    // Event handlers.
  protected:
    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu(this);
        menu.addActions({pass_act, undo_act, redo_act, home_act, end_act});
        menu.addSeparator();
        menu.addActions(
            {review_act, win_hint_act, ordinals_act, lock_stone_act});
        menu.addSeparator();
        menu.addActions({export_act, import_act});
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

        // Draw the stars.
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        double star_radius = grid_size / STAR_RADIUS_RATIO;
        for (Point pos : STAR_POSITIONS) {
            draw_circle(p, pos, star_radius);
        }

        // Draw the stones.
        double stone_radius = grid_size / STONE_RADIUS_RATIO;
        auto moves = game.past_moves();
        for (auto [pos, val] : moves) {
            p.setBrush(val == Stone::Black ? Qt::black : Qt::white);
            draw_circle(p, pos, stone_radius);
        }

        // Draw the win hint.
        auto win = game.first_win();
        if (shows_win_hint() && win) {
            double win_hint_width = grid_size / WIN_HINT_WIDTH_RATIO;
            p.setPen(QPen(Qt::red, win_hint_width, Qt::DotLine));

            auto [start, end] = win->row;
            p.drawLine(to_screen_pos(start), to_screen_pos(end));
        }

        if (shows_ordinals()) {
            // Draw the ordinals.
            QFont font("Arial", 64);
            font.setBold(true);
            QFontMetrics fm(font);

            // Calculate font sizes.
            QRect rects[3] = {fm.tightBoundingRect("0"),
                              fm.tightBoundingRect("00"),
                              fm.tightBoundingRect("000")};
            double font_sizes[3];
            double stone_diameter = stone_radius * 2.0;
            for (int i = 0; i < 3; i++) {
                int text_diameter =
                    std::max(rects[i].width(), rects[i].height());
                double ratio =
                    stone_diameter / text_diameter * ORDINAL_SIZE_RATIOS[i];
                font_sizes[i] = 64.0 * ratio;
            }

            for (usize i = 0; i < moves.size(); i++) {
                auto [pos, val] = moves[i];
                QPointF wpos = to_screen_pos(pos);
                QRectF stone_rect(wpos.x() - stone_radius,
                                  wpos.y() - stone_radius, stone_diameter,
                                  stone_diameter);

                QString ordinal = QString::number(i + 1);
                font.setPointSizeF(font_sizes[ordinal.size() - 1]);

                p.setPen(val == Stone::Black ? Qt::white : Qt::black);
                p.setFont(font);
                p.drawText(stone_rect, Qt::AlignCenter, ordinal);
            }
        } else if (!moves.empty()) {
            // Draw a star on the last stone placed.
            auto [pos, val] = moves.back();
            p.setPen(Qt::NoPen);
            p.setBrush(val == Stone::Black ? Qt::white : Qt::black);
            draw_circle(p, pos, star_radius);
        }

        // Draw the tentative move.
        p.setPen(Qt::NoPen);
        if (!reviewing() && filter_unoccupied(cursor_pos)) {
            p.setBrush(stone == Stone::Black ? Qt::black : Qt::white);
            p.setOpacity(TENTATIVE_MOVE_OPACITY);
            draw_circle(p, *cursor_pos, stone_radius);
        }
    }

    void hoverEvent(QSinglePointEvent *event) {
        auto pos = to_game_pos(event->position());
        bool should_repaint =
            filter_unoccupied(pos) != filter_unoccupied(cursor_pos);
        cursor_pos = pos;
        if (!reviewing() && should_repaint)
            repaint();
    }

    void enterEvent(QEnterEvent *event) override { hoverEvent(event); }
    void mouseMoveEvent(QMouseEvent *event) override { hoverEvent(event); }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton || reviewing())
            return;
        auto p = to_game_pos(event->position());
        if (!p)
            return;

        auto future = game.future_moves();
        if (!future.empty() &&
            !confirm(this, QString("覆盖未来的 %1 手棋").arg(future.size())))
            return;
        if (!game.make_move(*p, stone))
            return;

        if (!stone_locked())
            stone = opposite(stone);
        game_updated();
    }

    void wheelEvent(QWheelEvent *event) override {
        if (!reviewing())
            return;
        bool forward = event->angleDelta().y() > 0;
        if (forward ? game.redo() : game.undo())
            game_updated();
    }

    // Menu slots.
  private:
    void pass() {
        stone = opposite(stone);
        if (filter_unoccupied(cursor_pos))
            repaint();
    }

    void undo() {
        if (game.undo()) {
            infer_turn();
            game_updated();
        }
    }

    void redo() {
        if (game.redo()) {
            infer_turn();
            game_updated();
        }
    }

    void home() {
        if (game.jump(0)) {
            infer_turn();
            game_updated();
        }
    }

    void end() {
        if (game.jump(game.total_moves())) {
            infer_turn();
            game_updated();
        }
    }

    void toggle_review(bool enabled) {
        // We may have undone or redone moves in `wheelEvent`.
        if (!enabled)
            infer_turn();
        if (filter_unoccupied(cursor_pos))
            repaint();
    }

    void toggle_win_hint(bool enabled) {
        if (game.first_win())
            repaint();
    }

    void toggle_ordinals(bool enabled) {
        if (game.move_index() != 0)
            repaint();
    }

    void export_game() {
        QByteArray text = game.serialize()
                              .toBase64(QByteArray::Base64UrlEncoding)
                              .prepend(URI_PREFIX);
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(text);
    }

    void import_game() {
        QClipboard *clipboard = QApplication::clipboard();
        QByteArray text = clipboard->text().toUtf8().trimmed();
        if (!text.startsWith(URI_PREFIX))
            return import_failed(
                "合法的五子棋对局 URI 应以 \"gomoku://\" 起始。");

        auto data = QByteArray::fromBase64Encoding(
            text.sliced(URI_PREFIX.size()),
            QByteArray::Base64UrlEncoding |
                QByteArray::AbortOnBase64DecodingErrors);
        if (!data)
            return import_failed("Base64 解码失败。");

        if (auto res = Game::deserialize(*data)) {
            if (game.total_moves() != 0 &&
                !confirm(this, QString("导入 %1 手棋并完全覆盖当前对局")
                                   .arg(res->total_moves()))) {
                return;
            }
            game = *std::move(res);
            review_act->setChecked(true);
            game_updated();
        } else {
            import_failed("反序列化失败。");
        }
    }
};

class MainWindow : public QMainWindow {
  protected:
    void closeEvent(QCloseEvent *event) override {
        BoardWidget *widget = (BoardWidget *)centralWidget();
        if (widget->can_close_now() || confirm(this, "使您丢失未保存的对局"))
            event->accept();
        else
            event->ignore();
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    BoardWidget *widget = new BoardWidget();
    widget->setMouseTracking(true);

    MainWindow window;
    window.setCentralWidget(widget);
    window.setFixedSize(WINDOW_SIZE, WINDOW_SIZE);
    window.setWindowTitle("五子棋 (开局)");
    window.setWindowIcon(QIcon(":/resources/icon.ico"));
    window.show();

    return app.exec();
}
