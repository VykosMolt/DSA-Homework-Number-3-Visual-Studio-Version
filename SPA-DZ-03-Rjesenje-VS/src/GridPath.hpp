#ifndef SPA_DZ_03_GRID_PATH_HPP
#define SPA_DZ_03_GRID_PATH_HPP

#include <algorithm>
#include <fstream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace spa {

constexpr int kRows = 20;
constexpr int kCols = 40;

struct Point {
    int row = 0;
    int col = 0;
};

inline bool operator==(Point left, Point right) {
    return left.row == right.row && left.col == right.col;
}

inline bool operator!=(Point left, Point right) {
    return !(left == right);
}

using Grid = std::vector<std::string>;

struct SearchResult {
    std::vector<Point> path;
    std::vector<Point> visited;
};

inline bool inBounds(Point point) {
    return point.row >= 0 && point.row < kRows && point.col >= 0 && point.col < kCols;
}

inline Grid emptyGrid() {
    return Grid(kRows, std::string(kCols, '-'));
}

inline void addWalls(Grid& grid, const std::vector<Point>& walls) {
    for (Point wall : walls) {
        if (!inBounds(wall)) {
            throw std::out_of_range("Zid je izvan dozvoljenog igralista.");
        }
        grid[wall.row][wall.col] = '*';
    }
}

inline std::vector<Point> demoWalls() {
    std::vector<Point> walls;

    for (int row = 0; row < 10; ++row) {
        walls.push_back(Point{row, 14});
    }

    return walls;
}

inline std::string withoutComment(const std::string& line) {
    const std::size_t comment = line.find('#');
    if (comment == std::string::npos) {
        return line;
    }
    return line.substr(0, comment);
}

inline std::vector<Point> loadWalls(const std::string& fileName) {
    std::ifstream file(fileName);
    if (!file) {
        throw std::runtime_error("Ne mogu otvoriti datoteku sa zidovima: " + fileName);
    }

    std::vector<Point> walls;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;

        std::istringstream input(withoutComment(line));
        int row = 0;
        int col = 0;

        if (!(input >> row >> col)) {
            continue;
        }

        std::string extra;
        if (input >> extra) {
            throw std::runtime_error("Visak podataka u retku " + std::to_string(lineNumber) + ".");
        }

        if (row < 1 || row > kRows || col < 1 || col > kCols) {
            throw std::runtime_error("Zid u retku " + std::to_string(lineNumber) +
                                     " nije u rasponu igralista.");
        }

        walls.push_back(Point{row - 1, col - 1});
    }

    return walls;
}

inline void keepEndpointsWalkable(Grid& grid, Point start, Point goal) {
    grid[start.row][start.col] = '-';
    grid[goal.row][goal.col] = '-';
}

inline SearchResult dijkstraSearch(const Grid& grid, Point start, Point goal) {
    constexpr int kInfinity = std::numeric_limits<int>::max() / 4;

    struct Node {
        int distance = 0;
        Point point;
    };

    struct CompareNode {
        bool operator()(const Node& left, const Node& right) const {
            if (left.distance != right.distance) {
                return left.distance > right.distance;
            }
            if (left.point.row != right.point.row) {
                return left.point.row > right.point.row;
            }
            return left.point.col > right.point.col;
        }
    };

    std::vector<std::vector<int>> distance(kRows, std::vector<int>(kCols, kInfinity));
    std::vector<std::vector<Point>> previous(kRows, std::vector<Point>(kCols, Point{-1, -1}));
    std::priority_queue<Node, std::vector<Node>, CompareNode> queue;

    distance[start.row][start.col] = 0;
    queue.push(Node{0, start});

    SearchResult result;

    const std::vector<Point> directions = {
        Point{0, 1},
        Point{1, 0},
        Point{0, -1},
        Point{-1, 0},
    };

    while (!queue.empty()) {
        const Node current = queue.top();
        queue.pop();

        if (current.distance != distance[current.point.row][current.point.col]) {
            continue;
        }

        result.visited.push_back(current.point);

        if (current.point == goal) {
            break;
        }

        for (Point direction : directions) {
            const Point next{current.point.row + direction.row, current.point.col + direction.col};

            if (!inBounds(next) || grid[next.row][next.col] == '*') {
                continue;
            }

            const int newDistance = current.distance + 1;
            if (newDistance < distance[next.row][next.col]) {
                distance[next.row][next.col] = newDistance;
                previous[next.row][next.col] = current.point;
                queue.push(Node{newDistance, next});
            }
        }
    }

    if (distance[goal.row][goal.col] == kInfinity) {
        return result;
    }

    for (Point current = goal; current != Point{-1, -1}; current = previous[current.row][current.col]) {
        result.path.push_back(current);

        if (current == start) {
            break;
        }
    }

    std::reverse(result.path.begin(), result.path.end());
    return result;
}

}  // namespace spa

#endif  // SPA_DZ_03_GRID_PATH_HPP
