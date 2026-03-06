#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace heterosta_topk {

struct Simple3BendRouterConfig {
	std::string net_file;
	std::string cap_file;
	std::string output_spef;
	std::string design_name;
	double resistance_per_grid = 0.02;
	double capacitance_per_grid = 0.001;
	std::size_t max_nets = 0;
	std::size_t progress_interval = 5000;
	bool verbose = true;
};

struct Simple3BendRouterStats {
	std::uint32_t num_layers = 0;
	std::uint32_t num_gcell_x = 0;
	std::uint32_t num_gcell_y = 0;
	std::size_t total_nets = 0;
	std::size_t routed_nets = 0;
	std::size_t skipped_nets = 0;
	std::size_t total_pins = 0;
	std::size_t total_segments = 0;
	double total_wirelength = 0.0;
};

bool route_3bend_and_write_spef(const Simple3BendRouterConfig &config,
								Simple3BendRouterStats *stats = nullptr,
								std::string *error_message = nullptr);

}  // namespace heterosta_topk