#include <iostream>
#include <string>

#include "heterosta_topk/simple_3bend_router.hpp"

int main() {
	heterosta_topk::Simple3BendRouterConfig cfg;
	cfg.net_file = "/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/ariane/ariane.net";
	cfg.cap_file = "/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/ariane/ariane.cap";
	cfg.output_spef = "/home/mxchen/Routing/HeteroSTA-Topk/output/ariane_3bend.spef";
	cfg.design_name = "ariane_3bend";
	cfg.resistance_per_grid = 0.02;
	cfg.capacitance_per_grid = 0.001;
	cfg.max_nets = 200;
	cfg.progress_interval = 50;
	cfg.verbose = true;

	std::cout << "[INFO] Generating SPEF with the simple 3-bend router." << std::endl;
	std::cout << "[INFO] Set cfg.max_nets = 0 in src/simple_router_main.cpp if you want the full benchmark." << std::endl;

	heterosta_topk::Simple3BendRouterStats stats;
	std::string error_message;
	if (!heterosta_topk::route_3bend_and_write_spef(cfg, &stats, &error_message)) {
		std::cerr << error_message << std::endl;
		return 1;
	}

	std::cout << "[router] output_spef=" << cfg.output_spef << std::endl;
	std::cout << "[router] grid=" << stats.num_layers << " layers, "
				  << stats.num_gcell_x << " x " << stats.num_gcell_y << std::endl;
	std::cout << "[router] total_nets=" << stats.total_nets
				  << " routed_nets=" << stats.routed_nets
				  << " skipped_nets=" << stats.skipped_nets << std::endl;
	std::cout << "[router] total_pins=" << stats.total_pins
				  << " total_segments=" << stats.total_segments
				  << " total_wirelength=" << stats.total_wirelength << std::endl;
	return 0;
}