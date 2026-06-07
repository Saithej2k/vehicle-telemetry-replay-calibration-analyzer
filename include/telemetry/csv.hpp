#pragma once

#include <istream>
#include <string>
#include <unordered_map>
#include <vector>

namespace telemetry {

using CsvRow = std::unordered_map<std::string, std::string>;

std::string trim(std::string value);
std::vector<std::string> split_csv_line(const std::string& line);

class CsvReader {
public:
    explicit CsvReader(std::istream& input);

    [[nodiscard]] bool read(CsvRow& row);

private:
    std::istream& input_;
    std::vector<std::string> headers_;
};

}  // namespace telemetry
