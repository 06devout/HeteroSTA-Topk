#include <iostream>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

#include "heterosta_topk/simple_3bend_router.hpp"
#include "heterosta_topk/topk_api.hpp"

namespace {

std::string trim_copy(const std::string &text) {
	std::size_t begin = 0;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
		++begin;
	}
	std::size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
		--end;
	}
	return text.substr(begin, end - begin);
}

bool write_sanitized_ariane_sdc(const std::string &input_sdc,
								 const std::string &output_sdc,
								 std::string *error_message) {
	std::ifstream fin(input_sdc);
	if (!fin) {
		if (error_message != nullptr) {
			*error_message = "Failed to open original SDC file.";
		}
		return false;
	}
	std::ofstream fout(output_sdc);
	if (!fout) {
		if (error_message != nullptr) {
			*error_message = "Failed to open sanitized SDC output file.";
		}
		return false;
	}

	std::string line;
	while (std::getline(fin, line)) {
		const std::string trimmed = trim_copy(line);
		if (trimmed.rfind("set_clock_gating_check", 0) == 0) {
			continue;
		}
		if (trimmed.rfind("# set_disable_timing ", 0) == 0) {
			fout << trimmed.substr(2) << "\n";
			continue;
		}
		fout << line << "\n";
	}

	if (error_message != nullptr) {
		error_message->clear();
	}
	return true;
}

}  // namespace

int main() {
	heterosta_topk::Simple3BendRouterConfig route_cfg;
	route_cfg.net_file = "/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/ariane/ariane.net";
	route_cfg.cap_file = "/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/ariane/ariane.cap";
	route_cfg.output_spef = "/home/mxchen/Routing/HeteroSTA-Topk/output/ariane_3bend.spef";
	route_cfg.design_name = "ariane";
	route_cfg.resistance_per_grid = 0.02;
	route_cfg.capacitance_per_grid = 0.001;
	route_cfg.max_nets = 0;
	route_cfg.progress_interval = 50;
	route_cfg.verbose = true;

	std::cout << "[INFO] Step 1/2: run simple 3-bend router and generate SPEF." << std::endl;
	heterosta_topk::Simple3BendRouterStats route_stats;
	std::string error_message;
	if (!heterosta_topk::route_3bend_and_write_spef(route_cfg, &route_stats, &error_message)) {
		std::cerr << error_message << std::endl;
		return 1;
	}

	std::cout << "[router] output_spef=" << route_cfg.output_spef << std::endl;
	std::cout << "[router] routed_nets=" << route_stats.routed_nets
				  << " total_wirelength=" << route_stats.total_wirelength << std::endl;

	const std::string original_sdc = "/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/ariane/ariane.sdc";
	const std::string sanitized_sdc = "/home/mxchen/Routing/HeteroSTA-Topk/output/ariane_sanitized.sdc";
	if (!write_sanitized_ariane_sdc(original_sdc, sanitized_sdc, &error_message)) {
		std::cerr << error_message << std::endl;
		return 1;
	}
	std::cout << "[INFO] Using sanitized SDC: " << sanitized_sdc << std::endl;

	heterosta_topk::TopkConfig cfg;
	cfg.early_libs = {
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/NangateOpenCellLibrary_typical.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_128x116.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_128x256.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_128x32.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_256x16.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_256x32.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_256x48.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_256x64.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_32x32.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_512x64.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_64x124.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_64x256.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_64x62.lib",
		"/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/NanGate45/lib/fakeram45_64x64.lib",
	};
	cfg.late_libs = cfg.early_libs;
	cfg.netlist = "/home/mxchen/Routing/HeLEM-GR/data/ISPD2025/ariane/ariane.v";
	cfg.spef = route_cfg.output_spef;
	cfg.sdc = sanitized_sdc;
	cfg.top_module = "ariane";
	cfg.dump_file = "/home/mxchen/Routing/HeteroSTA-Topk/output/ariane_topk_max.rpt";
	cfg.num_paths = 20;
	cfg.nworst = 2;
	cfg.slack_lesser_than = 0.0f;
	cfg.split_endpoint_rf = false;
	cfg.report_max = true;
	cfg.use_cuda = false;
	cfg.device_id = -1;

	std::cout << "[INFO] Step 2/2: run top-k STA on the generated SPEF." << std::endl;

	heterosta_topk::TopkReport report;
	error_message.clear();
	if (!heterosta_topk::report_topk_critical_paths(cfg, &report, &error_message)) {
		std::cerr << error_message << std::endl;
		return 1;
	}

	std::cout << "mode=" << (cfg.report_max ? "max" : "min")
						<< " WNS=" << report.wns << " TNS=" << report.tns << std::endl;

	std::cout << "collected_paths=" << report.paths.size() << std::endl;
	for (size_t i = 0; i < report.paths.size(); ++i) {
		const auto &path = report.paths[i];
		std::cout << "path[" << i << "] slack=" << path.slack << " pins=";
		for (size_t k = 0; k < path.points.size(); ++k) {
			const auto &point = path.points[k];
			if (!point.name.empty()) {
				std::cout << point.name << (point.is_fall ? "F" : "R");
			} else {
				std::cout << point.pin_id << (point.is_fall ? "F" : "R");
			}
			if (k + 1 < path.points.size()) {
				std::cout << "->";
			}
		}
		std::cout << std::endl;
	}

	return 0;
}
