#include "telemetry/csv.hpp"

#include <algorithm>
#include <cctype>

namespace telemetry {

std::string trim(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char c = line[index];
        if (c == '"') {
            if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
                cell.push_back('"');
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (c == ',' && !quoted) {
            cells.push_back(trim(cell));
            cell.clear();
        } else {
            cell.push_back(c);
        }
    }

    cells.push_back(trim(cell));
    return cells;
}

CsvReader::CsvReader(std::istream& input) : input_(input) {
    std::string header_line;
    while (std::getline(input_, header_line)) {
        if (trim(header_line).empty()) {
            continue;
        }
        headers_ = split_csv_line(header_line);
        break;
    }
}

bool CsvReader::read(CsvRow& row) {
    row.clear();
    std::string line;
    while (std::getline(input_, line)) {
        if (trim(line).empty()) {
            continue;
        }
        const auto cells = split_csv_line(line);
        for (std::size_t index = 0; index < headers_.size(); ++index) {
            row[headers_[index]] = index < cells.size() ? cells[index] : "";
        }
        return true;
    }
    return false;
}

}  // namespace telemetry
