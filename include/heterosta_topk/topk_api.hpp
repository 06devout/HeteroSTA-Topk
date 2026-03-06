#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace heterosta_topk {

struct TopkConfig {
	std::string early_lib;
	std::string late_lib;
	std::vector<std::string> early_libs;
	std::vector<std::string> late_libs;
	std::string netlist;
	std::string spef;
	std::string sdc;
	std::string top_module;
	std::string dump_file;
	uintptr_t num_paths = 20;
	uintptr_t nworst = 1;
	float slack_lesser_than = 0.0f;
	bool split_endpoint_rf = true;
	bool report_max = true;
	bool use_cuda = false;
	int device_id = -1;
};

struct PathPoint {
	uintptr_t pin_id = 0;
	bool is_fall = false;
	std::string name;
};

struct PathReport {
	float slack = 0.0f;
	std::vector<PathPoint> points;
};

struct TopkReport {
	float wns = 0.0f;
	float tns = 0.0f;
	std::vector<PathReport> paths;
};

bool report_topk_critical_paths(const TopkConfig &config,
								TopkReport *report,
								std::string *error_message = nullptr);

}  // namespace heterosta_topk