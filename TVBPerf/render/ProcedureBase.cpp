#include "ProcedureBase.h"

#include "scene/SceneLoader.h"

namespace rndr {

	void ProcedureBase::init(util::ProgramArgument arg) {
		program_arguments_ = arg;
		frame_counter_.init(arg.warmup_frames, arg.measure_frames, 60);

        scene_cpu_ = scene::SceneLoader::load(program_arguments_);

        /*scene::SceneFingerprint::write_csv(
            get_scene_fingerprint_output_path(program_arguments_.output_filepath),
            *scene_cpu_,
            program_arguments_);*/
	}

	void ProcedureBase::close() {
        /*
        const std::string& path = program_arguments_.output_filepath;
        if (path == "") return;

       auto results = frame_counter_.summarize();

        std::string csv_string = util::ProgramArgument::get_header_string() + ",variant_name,pass_index,pass_name," +
            util::FrameCounter::CountedData::to_string_header() + "\n";

        const auto pass_infos = get_pass_output_infos(program_arguments_.renderer_variant);
        const std::string variant_name = get_renderer_variant_name(program_arguments_.renderer_variant);
        for (const auto& pass_info : pass_infos) {
            if (pass_info.index < 0 || static_cast<size_t>(pass_info.index) >= results.size()) continue;
            csv_string += program_arguments_.to_string() + ",";
            csv_string += variant_name + ",";
            csv_string += std::to_string(pass_info.index) + ",";
            csv_string += std::string(pass_info.name) + ",";
            csv_string += results[pass_info.index].to_string() + "\n";
        }

        std::ofstream ofs(path, std::ios::out | std::ios::trunc);
        if (!ofs) { std::cerr << "Failed to open output file: " << path << '\n'; return; }

        ofs << csv_string;
        if (!ofs) { std::cerr << "Failed to write output file: " << path << '\n'; return; }

        ofs.close();

        if (!ofs) { std::cerr << "Failed to close output file: " << path << '\n'; return; }
        std::cout << "Saved CSV: " << path << '\n';*/
	}
}