#include "heterosta_topk/simple_3bend_router.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace heterosta_topk {
namespace {

struct GridPoint {
	int x = 0;
	int y = 0;
};

struct NetPin {
	std::string name;
	int layer = 0;
	GridPoint point;
};

struct NetRecord {
	std::string name;
	std::vector<NetPin> pins;
};

struct ResistorEntry {
	std::string from;
	std::string to;
	double value = 0.0;
};

std::string trim(const std::string &value) {
	std::size_t begin = 0;
	while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
		++begin;
	}
	std::size_t end = value.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
		--end;
	}
	return value.substr(begin, end - begin);
}

void set_error(std::string *error_message, const std::string &message) {
	if (error_message != nullptr) {
		*error_message = message;
	}
}

std::string to_lower_copy(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return text;
}

std::vector<int> extract_integers(const std::string &text) {
	std::vector<int> values;
	std::string current;
	for (char ch : text) {
		if ((ch >= '0' && ch <= '9') || ch == '-') {
			current.push_back(ch);
		} else if (!current.empty()) {
			values.push_back(std::stoi(current));
			current.clear();
		}
	}
	if (!current.empty()) {
		values.push_back(std::stoi(current));
	}
	return values;
}

bool parse_cap_header(const std::string &cap_file, Simple3BendRouterStats *stats) {
	if (stats == nullptr) {
		return true;
	}
	std::ifstream fin(cap_file);
	if (!fin) {
		return false;
	}
	fin >> stats->num_layers >> stats->num_gcell_x >> stats->num_gcell_y;
	return static_cast<bool>(fin);
}

bool parse_pin_line(const std::string &line, NetPin *pin) {
	if (pin == nullptr) {
		return false;
	}
	const std::size_t first_comma = line.find(',');
	if (first_comma == std::string::npos) {
		return false;
	}
	pin->name = trim(line.substr(0, first_comma));
	const std::size_t bracket_pos = line.find('[', first_comma);
	if (bracket_pos == std::string::npos) {
		return false;
	}
	const std::vector<int> values = extract_integers(line.substr(bracket_pos));
	if (values.size() < 3) {
		return false;
	}
	pin->layer = values[0];
	pin->point.x = values[1];
	pin->point.y = values[2];
	return !pin->name.empty();
}

bool is_output_like_pin(const std::string &pin_name) {
	const std::string lower_name = to_lower_copy(pin_name);
	const std::size_t slash_pos = lower_name.rfind('/');
	const std::string tail = slash_pos == std::string::npos ? lower_name : lower_name.substr(slash_pos + 1);

	static const std::unordered_set<std::string> output_tokens = {
		"z", "zn", "q", "qn", "o", "y", "x", "so", "co", "s", "out"
	};
	if (output_tokens.count(tail) != 0) {
		return true;
	}
	if (lower_name.size() >= 2 && lower_name.substr(lower_name.size() - 2) == "_o") {
		return true;
	}
	if (lower_name.size() >= 2 && lower_name.substr(lower_name.size() - 2) == "_q") {
		return true;
	}
	if (lower_name.size() >= 2 && lower_name.substr(lower_name.size() - 2) == "_z") {
		return true;
	}
	return false;
}

char guess_direction(const NetPin &pin) {
	return is_output_like_pin(pin.name) ? 'O' : 'I';
}

char connection_kind(const NetPin &pin) {
	return pin.name.find('/') == std::string::npos ? 'P' : 'I';
}

int manhattan_distance(const GridPoint &lhs, const GridPoint &rhs) {
	return std::abs(lhs.x - rhs.x) + std::abs(lhs.y - rhs.y);
}

void append_point_if_needed(std::vector<GridPoint> *points, const GridPoint &point) {
	if (points == nullptr) {
		return;
	}
	if (points->empty() || points->back().x != point.x || points->back().y != point.y) {
		points->push_back(point);
	}
}

std::vector<GridPoint> build_3bend_route(const GridPoint &src, const GridPoint &dst) {
	std::vector<GridPoint> route;
	append_point_if_needed(&route, src);

	if (src.x == dst.x || src.y == dst.y) {
		append_point_if_needed(&route, dst);
		return route;
	}

	const int dx = dst.x - src.x;
	const int dy = dst.y - src.y;
	const int step_x = dx > 0 ? 1 : -1;
	const int step_y = dy > 0 ? 1 : -1;
	const int bend_x = src.x + step_x * std::max(1, std::abs(dx) / 2);
	const int bend_y = src.y + step_y * std::max(1, std::abs(dy) / 2);

	append_point_if_needed(&route, GridPoint{bend_x, src.y});
	append_point_if_needed(&route, GridPoint{bend_x, bend_y});
	append_point_if_needed(&route, GridPoint{dst.x, bend_y});
	append_point_if_needed(&route, dst);
	return route;
}

void append_unique_name(std::vector<std::string> *ordered_names,
						std::unordered_set<std::string> *name_set,
						const std::string &name) {
	if (ordered_names == nullptr || name_set == nullptr) {
		return;
	}
	if (name_set->insert(name).second) {
		ordered_names->push_back(name);
	}
}

bool write_one_net(std::ofstream &fout,
				const NetRecord &net,
				const Simple3BendRouterConfig &config,
				Simple3BendRouterStats *stats) {
	if (net.pins.size() < 2) {
		if (stats != nullptr) {
			++stats->skipped_nets;
		}
		return true;
	}

	const NetPin &root = net.pins.front();
	std::unordered_map<std::string, double> cap_by_node;
	std::vector<std::string> cap_order;
	std::unordered_set<std::string> cap_seen;
	std::vector<ResistorEntry> resistors;
	std::vector<std::string> internal_nodes;
	std::size_t internal_index = 1;

	for (const auto &pin : net.pins) {
		append_unique_name(&cap_order, &cap_seen, pin.name);
		cap_by_node[pin.name] += 0.0;
	}

	for (std::size_t i = 1; i < net.pins.size(); ++i) {
		const NetPin &sink = net.pins[i];
		const std::vector<GridPoint> route = build_3bend_route(root.point, sink.point);
		if (route.size() < 2) {
			continue;
		}

		std::vector<std::string> nodes;
		nodes.push_back(root.name);
		for (std::size_t p = 1; p + 1 < route.size(); ++p) {
			const std::string node_name = net.name + "/" + std::to_string(internal_index++);
			nodes.push_back(node_name);
			internal_nodes.push_back(node_name);
			append_unique_name(&cap_order, &cap_seen, node_name);
			cap_by_node[node_name] += 0.0;
		}
		nodes.push_back(sink.name);

		for (std::size_t p = 0; p + 1 < route.size(); ++p) {
			const int seg_len = manhattan_distance(route[p], route[p + 1]);
			if (seg_len <= 0) {
				continue;
			}
			const double resistance = static_cast<double>(seg_len) * config.resistance_per_grid;
			const double capacitance = static_cast<double>(seg_len) * config.capacitance_per_grid;
			cap_by_node[nodes[p]] += capacitance * 0.5;
			cap_by_node[nodes[p + 1]] += capacitance * 0.5;
			resistors.push_back(ResistorEntry{nodes[p], nodes[p + 1], resistance});

			if (stats != nullptr) {
				++stats->total_segments;
				stats->total_wirelength += static_cast<double>(seg_len);
			}
		}
	}

	double total_cap = 0.0;
	for (const auto &name : cap_order) {
		total_cap += cap_by_node[name];
	}

	fout << "*D_NET " << net.name << " " << total_cap << "\n";
	fout << "*CONN\n";
	for (const auto &pin : net.pins) {
		fout << "*" << connection_kind(pin) << " " << pin.name << " " << guess_direction(pin) << "\n";
	}
	fout << "*CAP\n";
	std::size_t cap_index = 1;
	for (const auto &name : cap_order) {
		fout << cap_index++ << " " << name << " " << cap_by_node[name] << "\n";
	}
	fout << "*RES\n";
	std::size_t res_index = 1;
	for (const auto &resistor : resistors) {
		fout << res_index++ << " " << resistor.from << " " << resistor.to << " " << resistor.value << "\n";
	}
	fout << "*END\n\n";

	if (stats != nullptr) {
		++stats->routed_nets;
		stats->total_pins += net.pins.size();
	}
	return true;
}

}  // namespace

bool route_3bend_and_write_spef(const Simple3BendRouterConfig &config,
								Simple3BendRouterStats *stats,
								std::string *error_message) {
	if (config.net_file.empty()) {
		set_error(error_message, "Simple3BendRouterConfig.net_file is empty.");
		return false;
	}
	if (config.cap_file.empty()) {
		set_error(error_message, "Simple3BendRouterConfig.cap_file is empty.");
		return false;
	}
	if (config.output_spef.empty()) {
		set_error(error_message, "Simple3BendRouterConfig.output_spef is empty.");
		return false;
	}

	Simple3BendRouterStats local_stats;
	Simple3BendRouterStats *final_stats = stats != nullptr ? stats : &local_stats;
	*final_stats = Simple3BendRouterStats{};

	if (!parse_cap_header(config.cap_file, final_stats)) {
		set_error(error_message, "Failed to read cap header.");
		return false;
	}

	std::ifstream fin(config.net_file);
	if (!fin) {
		set_error(error_message, "Failed to open net file.");
		return false;
	}
	std::ofstream fout(config.output_spef);
	if (!fout) {
		set_error(error_message, "Failed to open output SPEF file.");
		return false;
	}

	const std::string design_name = config.design_name.empty() ? "simple_3bend" : config.design_name;
	fout << "*SPEF \"IEEE 1481-1998\"\n";
	fout << "*DESIGN \"" << design_name << "\"\n";
	fout << "*DATE \"Generated by HeteroSTA-Topk simple 3-bend router\"\n";
	fout << "*VENDOR \"HeteroSTA-Topk\"\n";
	fout << "*PROGRAM \"simple_3bend_router\"\n";
	fout << "*VERSION \"0.1\"\n";
	fout << "*DESIGN_FLOW \"NETLIST_TYPE_VERILOG\"\n";
	fout << "*DIVIDER /\n";
	fout << "*DELIMITER /\n";
	fout << "*BUS_DELIMITER [ ]\n";
	fout << "*T_UNIT 1 PS\n";
	fout << "*C_UNIT 1 FF\n";
	fout << "*R_UNIT 1 KOHM\n";
	fout << "*L_UNIT 1 UH\n\n";

	NetRecord current_net;
	bool inside_net = false;
	std::string line;
	while (std::getline(fin, line)) {
		const std::string trimmed = trim(line);
		if (trimmed.empty()) {
			continue;
		}
		if (!inside_net) {
			if (trimmed == "(") {
				inside_net = true;
				continue;
			}
			current_net = NetRecord{};
			current_net.name = trimmed;
			continue;
		}
		if (trimmed == ")") {
			++final_stats->total_nets;
			bool routed_current_net = false;
			if (config.max_nets == 0 || final_stats->routed_nets < config.max_nets) {
				if (!write_one_net(fout, current_net, config, final_stats)) {
					set_error(error_message, "Failed to write routed net to SPEF.");
					return false;
				}
				routed_current_net = current_net.pins.size() >= 2;
			} else {
				++final_stats->skipped_nets;
			}

			if (config.verbose && routed_current_net && final_stats->routed_nets > 0 &&
					config.progress_interval > 0 &&
					(final_stats->routed_nets % config.progress_interval) == 0) {
				std::cout << "[router] routed_nets=" << final_stats->routed_nets
						  << " total_wirelength=" << final_stats->total_wirelength << std::endl;
			}

			inside_net = false;
			current_net = NetRecord{};
			if (config.max_nets > 0 && final_stats->routed_nets >= config.max_nets) {
				break;
			}
			continue;
		}

		NetPin pin;
		if (parse_pin_line(trimmed, &pin)) {
			current_net.pins.push_back(std::move(pin));
		}
	}

	set_error(error_message, "");
	return true;
}

}  // namespace heterosta_topk