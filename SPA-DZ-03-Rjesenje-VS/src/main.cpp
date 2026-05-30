#include "GridPath.hpp"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr unsigned kWindowWidth = 1280;
constexpr unsigned kWindowHeight = 720;
constexpr float kCell = 24.0F;
constexpr float kGridX = 32.0F;
constexpr float kGridY = 118.0F;
constexpr float kPanelX = 1032.0F;

const sf::Color kBackground(23, 24, 27);
const sf::Color kPanel(34, 36, 40);
const sf::Color kCellEmpty(50, 54, 61);
const sf::Color kCellAlt(45, 49, 56);
const sf::Color kGridLine(74, 80, 90);
const sf::Color kWall(17, 18, 21);
const sf::Color kVisited(58, 142, 190, 155);
const sf::Color kPath(248, 190, 76);
const sf::Color kPathTrail(222, 134, 53);
const sf::Color kStart(70, 210, 128);
const sf::Color kGoal(242, 87, 103);
const sf::Color kPlayer(249, 250, 252);
const sf::Color kText(235, 238, 243);
const sf::Color kMuted(166, 174, 185);
const sf::Color kButton(56, 61, 69);
const sf::Color kButtonHot(74, 82, 95);

enum class EditMode {
    Walls,
    Start,
    Goal,
};

enum class Playback {
    Idle,
    Exploring,
    Walking,
    Finished,
};

enum class Action {
    ModeWalls,
    ModeStart,
    ModeGoal,
    Run,
    Clear,
    Demo,
    Slower,
    Faster,
};

struct Button {
    sf::FloatRect bounds;
    std::string label;
    Action action;
};

struct AppOptions {
    bool help = false;
    bool checkOnly = false;
    std::string wallsFile;
};

AppOptions parseAppOptions(int argc, char* argv[]) {
    AppOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--check") {
            options.checkOnly = true;
        } else if (argument == "--walls") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Opcija --walls trazi putanju do datoteke.");
            }
            options.wallsFile = argv[++i];
        } else if (argument.rfind("--walls=", 0) == 0) {
            options.wallsFile = argument.substr(8);
        } else {
            throw std::invalid_argument("Nepoznata opcija: " + argument);
        }
    }

    return options;
}

void printHelp(const char* programName) {
    std::cout << "Upotreba: " << programName << " [opcije]\n"
              << "Opcije:\n"
              << "  --walls DATOTEKA   Ucitaj pocetne zidove iz datoteke.\n"
              << "  --check            Provjeri Dijkstrin put bez otvaranja prozora.\n"
              << "  -h, --help         Prikazi ovu pomoc.\n";
}

bool loadFont(sf::Font& font) {
    const std::array<const char*, 6> candidates = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };

    for (const char* path : candidates) {
        if (font.openFromFile(path)) {
            return true;
        }
    }

    return false;
}

std::vector<Button> makeButtons() {
    return {
        Button{{{kPanelX, 126.0F}, {70.0F, 34.0F}}, "Walls", Action::ModeWalls},
        Button{{{1110.0F, 126.0F}, {54.0F, 34.0F}}, "A", Action::ModeStart},
        Button{{{1172.0F, 126.0F}, {54.0F, 34.0F}}, "B", Action::ModeGoal},
        Button{{{kPanelX, 184.0F}, {194.0F, 40.0F}}, "Run Dijkstra", Action::Run},
        Button{{{kPanelX, 236.0F}, {92.0F, 34.0F}}, "Clear", Action::Clear},
        Button{{{1134.0F, 236.0F}, {92.0F, 34.0F}}, "Demo", Action::Demo},
        Button{{{kPanelX, 306.0F}, {92.0F, 34.0F}}, "Slower", Action::Slower},
        Button{{{1134.0F, 306.0F}, {92.0F, 34.0F}}, "Faster", Action::Faster},
    };
}

std::string modeName(EditMode mode) {
    if (mode == EditMode::Start) {
        return "setting A";
    }
    if (mode == EditMode::Goal) {
        return "setting B";
    }
    return "editing walls";
}

sf::Vector2f cellTopLeft(spa::Point point) {
    return {kGridX + static_cast<float>(point.col) * kCell,
            kGridY + static_cast<float>(point.row) * kCell};
}

std::optional<spa::Point> cellAt(sf::Vector2i pixel) {
    const float x = static_cast<float>(pixel.x) - kGridX;
    const float y = static_cast<float>(pixel.y) - kGridY;

    if (x < 0.0F || y < 0.0F) {
        return std::nullopt;
    }

    const int col = static_cast<int>(x / kCell);
    const int row = static_cast<int>(y / kCell);
    const spa::Point point{row, col};

    if (!spa::inBounds(point)) {
        return std::nullopt;
    }

    return point;
}

void drawText(sf::RenderWindow& window,
              const sf::Font& font,
              const std::string& value,
              unsigned size,
              sf::Vector2f position,
              sf::Color color = kText) {
    sf::Text text(font, value, size);
    text.setFillColor(color);
    text.setPosition(position);
    window.draw(text);
}

void drawCenteredText(sf::RenderWindow& window,
                      const sf::Font& font,
                      const std::string& value,
                      unsigned size,
                      sf::Vector2f center,
                      sf::Color color = kText) {
    sf::Text text(font, value, size);
    text.setFillColor(color);
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.0F,
                    bounds.position.y + bounds.size.y / 2.0F});
    text.setPosition(center);
    window.draw(text);
}

void drawPanelBox(sf::RenderWindow& window, sf::FloatRect bounds, sf::Color fill) {
    sf::RectangleShape box(bounds.size);
    box.setPosition(bounds.position);
    box.setFillColor(fill);
    box.setOutlineColor(sf::Color(82, 88, 98));
    box.setOutlineThickness(1.0F);
    window.draw(box);
}

void drawButton(sf::RenderWindow& window,
                const sf::Font& font,
                const Button& button,
                bool selected,
                bool hovered) {
    sf::RectangleShape shape(button.bounds.size);
    shape.setPosition(button.bounds.position);
    shape.setFillColor(selected ? sf::Color(77, 112, 92) : (hovered ? kButtonHot : kButton));
    shape.setOutlineColor(selected ? kStart : sf::Color(92, 99, 110));
    shape.setOutlineThickness(selected ? 2.0F : 1.0F);
    window.draw(shape);

    drawCenteredText(window,
                     font,
                     button.label,
                     15,
                     button.bounds.getCenter(),
                     selected ? sf::Color(240, 255, 244) : kText);
}

bool isModeButton(const Button& button, EditMode mode) {
    return (button.action == Action::ModeWalls && mode == EditMode::Walls) ||
           (button.action == Action::ModeStart && mode == EditMode::Start) ||
           (button.action == Action::ModeGoal && mode == EditMode::Goal);
}

spa::Grid makeInitialGrid(const AppOptions& options) {
    spa::Grid grid = spa::emptyGrid();
    const std::vector<spa::Point> walls =
        options.wallsFile.empty() ? spa::demoWalls() : spa::loadWalls(options.wallsFile);
    spa::addWalls(grid, walls);
    return grid;
}

void applyDemoWalls(spa::Grid& grid, spa::Point start, spa::Point goal) {
    grid = spa::emptyGrid();
    spa::addWalls(grid, spa::demoWalls());
    spa::keepEndpointsWalkable(grid, start, goal);
}

void clearWalls(spa::Grid& grid) {
    grid = spa::emptyGrid();
}

void runHeadlessCheck(const AppOptions& options) {
    spa::Grid grid = makeInitialGrid(options);
    const spa::Point start{0, 0};
    const spa::Point goal{0, 39};
    spa::keepEndpointsWalkable(grid, start, goal);

    const spa::SearchResult result = spa::dijkstraSearch(grid, start, goal);
    if (result.path.empty()) {
        throw std::runtime_error("Dijkstra nije pronasao put u demo provjeri.");
    }

    std::cout << "SFML provjera OK: put ima " << (result.path.size() - 1)
              << " koraka, obradjeno cvorova: " << result.visited.size() << ".\n";
}

void runApp(const AppOptions& options) {
    sf::Font font;
    if (!loadFont(font)) {
        throw std::runtime_error("Ne mogu ucitati font za SFML prikaz.");
    }

    sf::RenderWindow window(sf::VideoMode({kWindowWidth, kWindowHeight}),
                            "SPA DZ3 - Dijkstra pathfinder",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    const std::vector<Button> buttons = makeButtons();
    spa::Point start{0, 0};
    spa::Point goal{0, 39};
    spa::Grid grid = makeInitialGrid(options);
    spa::keepEndpointsWalkable(grid, start, goal);

    EditMode mode = EditMode::Walls;
    Playback playback = Playback::Idle;
    spa::SearchResult search;
    std::size_t visibleVisited = 0;
    std::size_t visiblePath = 0;
    float exploreAccumulator = 0.0F;
    float walkAccumulator = 0.0F;
    float pathDelay = 0.10F;
    bool leftDragging = false;
    bool dragAddsWalls = true;
    std::string status = "Press Space or Run Dijkstra.";
    sf::Clock frameClock;
    float totalTime = 0.0F;

    auto resetPlayback = [&]() {
        playback = Playback::Idle;
        search = spa::SearchResult{};
        visibleVisited = 0;
        visiblePath = 0;
        exploreAccumulator = 0.0F;
        walkAccumulator = 0.0F;
    };

    auto beginSearch = [&]() {
        spa::keepEndpointsWalkable(grid, start, goal);
        search = spa::dijkstraSearch(grid, start, goal);
        visibleVisited = 0;
        visiblePath = 0;
        exploreAccumulator = 0.0F;
        walkAccumulator = 0.0F;
        playback = Playback::Exploring;
        status = "Dijkstra is exploring the grid.";
    };

    auto applyCellEdit = [&](spa::Point point, bool addWall) {
        if (mode == EditMode::Start) {
            start = point;
            spa::keepEndpointsWalkable(grid, start, goal);
            resetPlayback();
            status = "A moved. Press Space to search.";
            return;
        }

        if (mode == EditMode::Goal) {
            goal = point;
            spa::keepEndpointsWalkable(grid, start, goal);
            resetPlayback();
            status = "B moved. Press Space to search.";
            return;
        }

        if (point == start || point == goal) {
            return;
        }

        grid[point.row][point.col] = addWall ? '*' : '-';
        resetPlayback();
        status = "Walls changed. Press Space to search.";
    };

    auto handleAction = [&](Action action) {
        switch (action) {
            case Action::ModeWalls:
                mode = EditMode::Walls;
                status = "Wall mode: left drag adds, right drag removes.";
                break;
            case Action::ModeStart:
                mode = EditMode::Start;
                status = "Click any cell to place A.";
                break;
            case Action::ModeGoal:
                mode = EditMode::Goal;
                status = "Click any cell to place B.";
                break;
            case Action::Run:
                beginSearch();
                break;
            case Action::Clear:
                clearWalls(grid);
                spa::keepEndpointsWalkable(grid, start, goal);
                resetPlayback();
                status = "Walls cleared. Press Space to search.";
                break;
            case Action::Demo:
                applyDemoWalls(grid, start, goal);
                resetPlayback();
                status = "Demo wall restored. Press Space to search.";
                break;
            case Action::Slower:
                pathDelay = std::min(0.50F, pathDelay + 0.025F);
                break;
            case Action::Faster:
                pathDelay = std::max(0.025F, pathDelay - 0.025F);
                break;
        }
    };

    while (window.isOpen()) {
        const float dt = frameClock.restart().asSeconds();
        totalTime += dt;

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    window.close();
                } else if (key->code == sf::Keyboard::Key::Space ||
                           key->code == sf::Keyboard::Key::Enter) {
                    beginSearch();
                } else if (key->code == sf::Keyboard::Key::W) {
                    handleAction(Action::ModeWalls);
                } else if (key->code == sf::Keyboard::Key::A) {
                    handleAction(Action::ModeStart);
                } else if (key->code == sf::Keyboard::Key::B) {
                    handleAction(Action::ModeGoal);
                } else if (key->code == sf::Keyboard::Key::C) {
                    handleAction(Action::Clear);
                } else if (key->code == sf::Keyboard::Key::D) {
                    handleAction(Action::Demo);
                } else if (key->code == sf::Keyboard::Key::Hyphen ||
                           key->code == sf::Keyboard::Key::Subtract) {
                    handleAction(Action::Slower);
                } else if (key->code == sf::Keyboard::Key::Equal ||
                           key->code == sf::Keyboard::Key::Add) {
                    handleAction(Action::Faster);
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
                bool handledButton = false;
                const sf::Vector2f mouseFloat(static_cast<float>(mouse->position.x),
                                              static_cast<float>(mouse->position.y));

                if (mouse->button == sf::Mouse::Button::Left) {
                    for (const Button& button : buttons) {
                        if (button.bounds.contains(mouseFloat)) {
                            handleAction(button.action);
                            handledButton = true;
                            break;
                        }
                    }
                }

                if (!handledButton) {
                    if (const std::optional<spa::Point> point = cellAt(mouse->position)) {
                        if (mouse->button == sf::Mouse::Button::Right) {
                            mode = EditMode::Walls;
                            applyCellEdit(*point, false);
                            leftDragging = true;
                            dragAddsWalls = false;
                        } else if (mouse->button == sf::Mouse::Button::Left) {
                            dragAddsWalls = mode != EditMode::Walls ||
                                            grid[point->row][point->col] != '*';
                            applyCellEdit(*point, dragAddsWalls);
                            leftDragging = mode == EditMode::Walls;
                        }
                    }
                }
            }

            if (event->is<sf::Event::MouseButtonReleased>()) {
                leftDragging = false;
            }

            if (const auto* moved = event->getIf<sf::Event::MouseMoved>()) {
                if (leftDragging && mode == EditMode::Walls) {
                    if (const std::optional<spa::Point> point = cellAt(moved->position)) {
                        applyCellEdit(*point, dragAddsWalls);
                    }
                }
            }
        }

        if (playback == Playback::Exploring) {
            exploreAccumulator += dt;
            const float exploreDelay = 0.010F;
            while (exploreAccumulator >= exploreDelay && visibleVisited < search.visited.size()) {
                exploreAccumulator -= exploreDelay;
                ++visibleVisited;
            }

            if (visibleVisited >= search.visited.size()) {
                if (search.path.empty()) {
                    playback = Playback::Finished;
                    status = "No path exists. Edit walls and search again.";
                } else {
                    playback = Playback::Walking;
                    visiblePath = 1;
                    status = "Shortest path found. Animating player.";
                }
            }
        } else if (playback == Playback::Walking) {
            walkAccumulator += dt;
            while (walkAccumulator >= pathDelay && visiblePath < search.path.size()) {
                walkAccumulator -= pathDelay;
                ++visiblePath;
            }

            if (visiblePath >= search.path.size()) {
                playback = Playback::Finished;
                status = "Arrived. Edit and press Space for another run.";
            }
        }

        std::vector<std::vector<bool>> visited(spa::kRows, std::vector<bool>(spa::kCols, false));
        for (std::size_t i = 0; i < std::min(visibleVisited, search.visited.size()); ++i) {
            visited[search.visited[i].row][search.visited[i].col] = true;
        }

        std::vector<std::vector<bool>> pathCells(spa::kRows, std::vector<bool>(spa::kCols, false));
        const std::size_t pathLimit = playback == Playback::Walking || playback == Playback::Finished
                                          ? std::min(visiblePath, search.path.size())
                                          : 0;
        for (std::size_t i = 0; i < pathLimit; ++i) {
            pathCells[search.path[i].row][search.path[i].col] = true;
        }

        window.clear(kBackground);

        sf::RectangleShape header({static_cast<float>(kWindowWidth), 84.0F});
        header.setPosition({0.0F, 0.0F});
        header.setFillColor(sf::Color(39, 42, 47));
        window.draw(header);

        drawText(window, font, "SPA DZ3 - Dijkstra Pathfinder", 28, {32.0F, 20.0F});
        drawText(window,
                 font,
                 "Interactive SFML version: edit walls, place A/B, run shortest path.",
                 16,
                 {34.0F, 56.0F},
                 kMuted);

        drawPanelBox(window, {{1012.0F, 104.0F}, {236.0F, 540.0F}}, kPanel);
        drawText(window, font, "Controls", 22, {kPanelX, 114.0F});

        const sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
        const sf::Vector2f mouseFloat(static_cast<float>(mousePixel.x), static_cast<float>(mousePixel.y));
        for (const Button& button : buttons) {
            drawButton(window,
                       font,
                       button,
                       isModeButton(button, mode),
                       button.bounds.contains(mouseFloat));
        }

        drawText(window, font, "Keyboard", 17, {kPanelX, 370.0F});
        drawText(window, font, "Space/Enter - run", 14, {kPanelX, 400.0F}, kMuted);
        drawText(window, font, "W/A/B - edit mode", 14, {kPanelX, 424.0F}, kMuted);
        drawText(window, font, "C - clear, D - demo", 14, {kPanelX, 448.0F}, kMuted);
        drawText(window, font, "+/- - speed", 14, {kPanelX, 472.0F}, kMuted);
        drawText(window, font, "Esc - close", 14, {kPanelX, 496.0F}, kMuted);

        drawText(window, font, "Status", 17, {kPanelX, 542.0F});
        drawText(window, font, "Mode: " + modeName(mode), 14, {kPanelX, 572.0F}, kMuted);
        drawText(window,
                 font,
                 "Speed: " + std::to_string(static_cast<int>(std::round(pathDelay * 1000.0F))) + " ms",
                 14,
                 {kPanelX, 596.0F},
                 kMuted);

        if (!search.path.empty()) {
            drawText(window,
                     font,
                     "Path: " + std::to_string(search.path.size() - 1) + " steps",
                     14,
                     {kPanelX, 620.0F},
                     kStart);
        } else if (playback == Playback::Finished) {
            drawText(window, font, "Path: blocked", 14, {kPanelX, 620.0F}, kGoal);
        } else {
            drawText(window, font, "Path: not run", 14, {kPanelX, 620.0F}, kMuted);
        }

        sf::RectangleShape gridBacking({spa::kCols * kCell + 12.0F, spa::kRows * kCell + 12.0F});
        gridBacking.setPosition({kGridX - 6.0F, kGridY - 6.0F});
        gridBacking.setFillColor(sf::Color(29, 31, 35));
        gridBacking.setOutlineColor(sf::Color(91, 98, 110));
        gridBacking.setOutlineThickness(1.0F);
        window.draw(gridBacking);

        for (int row = 0; row < spa::kRows; ++row) {
            for (int col = 0; col < spa::kCols; ++col) {
                const spa::Point point{row, col};
                sf::RectangleShape cell({kCell - 2.0F, kCell - 2.0F});
                cell.setPosition(cellTopLeft(point) + sf::Vector2f{1.0F, 1.0F});
                cell.setFillColor(((row + col) % 2 == 0) ? kCellEmpty : kCellAlt);
                cell.setOutlineColor(kGridLine);
                cell.setOutlineThickness(0.35F);

                if (grid[row][col] == '*') {
                    cell.setFillColor(kWall);
                    cell.setOutlineColor(sf::Color(96, 102, 112));
                    cell.setOutlineThickness(1.0F);
                } else if (visited[row][col]) {
                    cell.setFillColor(kVisited);
                }

                if (pathCells[row][col]) {
                    cell.setFillColor(kPathTrail);
                }

                if (point == start) {
                    cell.setFillColor(kStart);
                } else if (point == goal) {
                    cell.setFillColor(kGoal);
                }

                window.draw(cell);
            }
        }

        if (pathLimit > 0) {
            for (std::size_t i = 0; i < pathLimit; ++i) {
                const spa::Point point = search.path[i];
                sf::CircleShape dot(4.0F);
                dot.setOrigin({4.0F, 4.0F});
                dot.setPosition(cellTopLeft(point) + sf::Vector2f{kCell / 2.0F, kCell / 2.0F});
                dot.setFillColor(kPath);
                window.draw(dot);
            }
        }

        drawCenteredText(window,
                         font,
                         "A",
                         18,
                         cellTopLeft(start) + sf::Vector2f{kCell / 2.0F, kCell / 2.0F - 1.0F},
                         sf::Color(19, 42, 29));
        drawCenteredText(window,
                         font,
                         "B",
                         18,
                         cellTopLeft(goal) + sf::Vector2f{kCell / 2.0F, kCell / 2.0F - 1.0F},
                         sf::Color(54, 18, 27));

        if (playback == Playback::Walking && visiblePath > 0 && !search.path.empty()) {
            const spa::Point playerCell = search.path[std::min(visiblePath - 1, search.path.size() - 1)];
            const float pulse = 1.0F + 0.10F * std::sin(totalTime * 9.0F);
            sf::CircleShape halo(11.0F * pulse);
            halo.setOrigin({11.0F * pulse, 11.0F * pulse});
            halo.setPosition(cellTopLeft(playerCell) + sf::Vector2f{kCell / 2.0F, kCell / 2.0F});
            halo.setFillColor(sf::Color(255, 255, 255, 48));
            window.draw(halo);

            sf::CircleShape player(7.0F);
            player.setOrigin({7.0F, 7.0F});
            player.setPosition(cellTopLeft(playerCell) + sf::Vector2f{kCell / 2.0F, kCell / 2.0F});
            player.setFillColor(kPlayer);
            player.setOutlineColor(sf::Color(35, 38, 44));
            player.setOutlineThickness(2.0F);
            window.draw(player);
        }

        sf::RectangleShape footer({960.0F, 54.0F});
        footer.setPosition({kGridX, 626.0F});
        footer.setFillColor(sf::Color(33, 36, 40));
        footer.setOutlineColor(sf::Color(78, 86, 98));
        footer.setOutlineThickness(1.0F);
        window.draw(footer);
        drawText(window, font, status, 17, {kGridX + 18.0F, 642.0F}, kText);

        window.display();
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const AppOptions options = parseAppOptions(argc, argv);

        if (options.help) {
            printHelp(argv[0]);
            return 0;
        }

        if (options.checkOnly) {
            runHeadlessCheck(options);
            return 0;
        }

        runApp(options);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Greska: " << error.what() << '\n';
        return 1;
    }
}
