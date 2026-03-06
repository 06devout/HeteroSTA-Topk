#include "heterosta_topk/topk_api.hpp"

#include <cstdlib>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include "heterosta.h"

namespace heterosta_topk {
namespace {

extern "C" void cpp_log_callback(uint8_t level, const char *message) {
	const char *level_str = "UNKNW";
	switch (level) {
		case 1: level_str = "ERROR"; break;
		case 2: level_str = "WARN "; break;
		case 3: level_str = "INFO "; break;
		case 4: level_str = "DEBUG"; break;
		case 5: level_str = "TRACE"; break;
		default: break;
	}
	if (level <= 2) {
		std::cerr << "[" << level_str << "] " << message << std::endl;
	} else {
		std::cout << "[" << level_str << "] " << message << std::endl;
	}
}

void set_error(std::string *error_message, const std::string &message) {
	if (error_message != nullptr) {
		*error_message = message;
	}
}

bool validate_config(const TopkConfig &config, std::string *error_message) {
	if (config.early_lib.empty() && config.early_libs.empty()) {
		set_error(error_message, "Config.early_lib / Config.early_libs is empty.");
		return false;
	}
	if (config.late_lib.empty() && config.late_libs.empty()) {
		set_error(error_message, "Config.late_lib / Config.late_libs is empty.");
		return false;
	}
	if (config.netlist.empty()) {
		set_error(error_message, "Config.netlist is empty.");
		return false;
	}
	if (config.spef.empty()) {
		set_error(error_message, "Config.spef is empty.");
		return false;
	}
	if (config.sdc.empty()) {
		set_error(error_message, "Config.sdc is empty.");
		return false;
	}
	return true;
}

std::vector<std::string> collect_libs(const std::string &single_lib,
									  const std::vector<std::string> &multi_libs) {
	if (!multi_libs.empty()) {
		return multi_libs;
	}
	if (!single_lib.empty()) {
		return {single_lib};
	}
	return {};
}

bool initialize_heterosta(std::string *error_message) {
	static std::once_flag init_flag;
	static bool init_ok = false;
	static std::string init_error;

	std::call_once(init_flag, []() {
		heterosta_init_logger(cpp_log_callback);

		const char *license = std::getenv("HeteroSTA_Lic");
		if (license == nullptr || std::strlen(license) == 0) {
			init_error = "HeteroSTA_Lic is not set.";
			return;
		}

		if (!heterosta_init_license(license)) {
			init_error = "Failed to initialize HeteroSTA license.";
			return;
		}

		init_ok = true;
	});

	if (!init_ok) {
		set_error(error_message, init_error.empty() ? "Failed to initialize HeteroSTA." : init_error);
	}
	return init_ok;
}

bool starts_with(const std::string &text, const std::string &prefix) {
	return text.rfind(prefix, 0) == 0;
}

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

bool is_separator_line(const std::string &line) {
	const std::string trimmed = trim_copy(line);
	return !trimmed.empty() && trimmed.find_first_not_of('-') == std::string::npos;
}

bool parse_dumped_paths_file(const std::string &dump_file,
							 TopkReport *report,
							 std::string *error_message) {
	if (report == nullptr) {
		return true;
	}
	std::ifstream fin(dump_file);
	if (!fin) {
		set_error(error_message, "Failed to open dumped path report file.");
		return false;
	}

	report->paths.clear();
	PathReport *current_path = nullptr;
	bool in_point_table = false;
	bool waiting_for_rf_line = false;
	std::string pending_point_name;
	std::string line;
	while (std::getline(fin, line)) {
		const std::string trimmed = trim_copy(line);
		if (starts_with(trimmed, "Path ")) {
			std::istringstream iss(trimmed);
			std::string path_token;
			std::string index_token;
			std::string slack_token;
			double slack = 0.0;
			iss >> path_token >> index_token >> slack_token >> slack;
			report->paths.push_back(PathReport{});
			current_path = &report->paths.back();
			current_path->slack = static_cast<float>(slack);
			in_point_table = false;
			waiting_for_rf_line = false;
			pending_point_name.clear();
			continue;
		}

		if (current_path == nullptr) {
			continue;
		}
		if (starts_with(trimmed, "Point")) {
			in_point_table = true;
			waiting_for_rf_line = false;
			pending_point_name.clear();
			continue;
		}
		if (!in_point_table) {
			continue;
		}
		if (trimmed.empty() || is_separator_line(trimmed)) {
			continue;
		}
		if (starts_with(trimmed, "Data required time") || starts_with(trimmed, "Slack:") ||
				starts_with(trimmed, "Logic depth:") || starts_with(trimmed, "Logic avg fanout:")) {
			in_point_table = false;
			waiting_for_rf_line = false;
			pending_point_name.clear();
			continue;
		}

		if (!waiting_for_rf_line) {
			pending_point_name = trimmed;
			waiting_for_rf_line = true;
			continue;
		}

		std::size_t pos = 0;
		while (pos < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[pos])) != 0) {
			++pos;
		}
		if (pos >= trimmed.size()) {
			continue;
		}
		const char rf = trimmed[pos];
		if (rf == 'r' || rf == 'f' || rf == 'R' || rf == 'F') {
			current_path->points.push_back(PathPoint{
				.pin_id = 0,
				.is_fall = (rf == 'f' || rf == 'F'),
				.name = pending_point_name,
			});
		}
		waiting_for_rf_line = false;
		pending_point_name.clear();
	}

	set_error(error_message, "");
	return true;
}

}  // namespace

bool report_topk_critical_paths(const TopkConfig &config,
								TopkReport *report,
								std::string *error_message) {
	if (!validate_config(config, error_message)) {
		return false;
	}
	if (!initialize_heterosta(error_message)) {
		return false;
	}

	TopkReport local_report;
	TopkReport *final_report = report != nullptr ? report : &local_report;
	final_report->wns = 0.0f;
	final_report->tns = 0.0f;
	final_report->paths.clear();

	STAHoldings *sta = nullptr;
	if (config.device_id >= 0) {
		sta = heterosta_new_with_device(static_cast<uint8_t>(config.device_id));
	} else {
		sta = heterosta_new();
	}

	if (sta == nullptr) {
		set_error(error_message, "Failed to create STAHoldings.");
		return false;
	}

	auto fail = [&](const std::string &message) {
		set_error(error_message, message);
		heterosta_free(sta);
		return false;
	};

	heterosta_set_delay_calculator_arnoldi(sta);

	const std::vector<std::string> early_libs = collect_libs(config.early_lib, config.early_libs);
	const std::vector<std::string> late_libs = collect_libs(config.late_lib, config.late_libs);
	for (const auto &lib : early_libs) {
		if (!heterosta_read_liberty(sta, EARLY, lib.c_str())) {
			return fail("Failed to read EARLY liberty.");
		}
	}
	for (const auto &lib : late_libs) {
		if (!heterosta_read_liberty(sta, LATE, lib.c_str())) {
			return fail("Failed to read LATE liberty.");
		}
	}

	const char *top_module = config.top_module.empty() ? nullptr : config.top_module.c_str();
	if (!heterosta_read_netlist(sta, config.netlist.c_str(), top_module)) {
		return fail("Failed to read netlist.");
	}
	if (!heterosta_read_spef(sta, config.spef.c_str())) {
		return fail("Failed to read spef.");
	}

	heterosta_flatten_all(sta);
	heterosta_build_graph(sta);
	heterosta_zero_slew(sta);

	if (!heterosta_read_sdc(sta, config.sdc.c_str())) {
		return fail("Failed to read sdc.");
	}

	heterosta_update_delay(sta, config.use_cuda);
	heterosta_update_arrivals(sta, config.use_cuda);

	if (config.report_max) {
		heterosta_report_wns_tns_max(sta, &final_report->wns, &final_report->tns, config.use_cuda);
	} else {
		heterosta_report_wns_tns_min(sta, &final_report->wns, &final_report->tns, config.use_cuda);
	}

	if (!config.dump_file.empty()) {
		if (config.report_max) {
			heterosta_dump_paths_max_to_file(sta,
					config.num_paths,
					config.nworst,
					config.dump_file.c_str(),
					config.use_cuda);
		} else {
			heterosta_dump_paths_min_to_file(sta,
					config.num_paths,
					config.nworst,
					config.dump_file.c_str(),
					config.use_cuda);
		}

		if (!parse_dumped_paths_file(config.dump_file, final_report, error_message)) {
			return fail("Failed to parse dumped path report.");
		}
	} else {
		PBAPathCollectionCppInterface *paths = heterosta_report_paths(
				sta,
				config.num_paths,
				config.nworst,
				config.split_endpoint_rf,
				config.slack_lesser_than,
				config.report_max,
				config.use_cuda,
				false);

		if (paths == nullptr) {
			return fail("Failed to collect paths.");
		}

		final_report->paths.reserve(paths->num_paths);
		for (uintptr_t i = 0; i < paths->num_paths; ++i) {
			PathReport path_report;
			path_report.slack = paths->slacks[i];

			const uintptr_t st = paths->path_st[i];
			const uintptr_t ed = paths->path_st[i + 1];
			path_report.points.reserve(ed - st);
			for (uintptr_t k = st; k < ed; ++k) {
				const uintptr_t pin_rf = paths->pin_rfs[k];
				path_report.points.push_back(PathPoint{
					.pin_id = pin_rf >> 1,
					.is_fall = static_cast<bool>(pin_rf & 1u),
					.name = "",
				});
			}

			final_report->paths.push_back(std::move(path_report));
		}

		heterosta_free_pba_path_collection(paths);
	}

	heterosta_free(sta);
	set_error(error_message, "");
	return true;
}

}  // namespace heterosta_topk