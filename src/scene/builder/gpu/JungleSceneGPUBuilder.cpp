#include "scene/builder/gpu/JungleSceneGPUBuilder.h"

#include "scene/builder/gpu/BenchmarkSceneGPUBuilder.h"
#include "scene/builder/gpu/DonutSceneGPUBuilder.h"
#include "util/Logger.h"

namespace scene {

    BenchmarkSceneGPUData JungleSceneGPUBuilder::build_benchmark(
        const SceneCPUData& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
            used_upload_heaps,
        bool load_textures) {

        util::Logger::g_logger <<
            "Jungle GPU entry: benchmark generic layout upload; vertices=" <<
            source.vertices.size() <<
            ", indices=" << source.indices.size() <<
            ", instances=" << source.instances.size() <<
            ", draw_instances=" << source.draw_instances.size() << ".\n";
        BenchmarkSceneGPUData result =
            BenchmarkSceneGPUBuilder::build(
            source,
            device,
            command_list,
            used_upload_heaps,
            load_textures);
        util::Logger::g_logger <<
            "Jungle GPU upload complete: benchmark vertices=" <<
            result.vertex_count <<
            ", indices=" << result.index_count <<
            ", instances=" << result.instance_count <<
            ", draw_instances=" << result.draw_instance_count << ".\n";
        return result;
    }

    DonutSceneGPUData JungleSceneGPUBuilder::build_donut(
        const SceneCPUData& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
            used_upload_heaps,
        const std::function<void()>& flush_uploads,
        bool load_textures) {

        util::Logger::g_logger <<
            "Jungle GPU entry: Donut generic layout upload; vertices=" <<
            source.vertices.size() <<
            ", indices=" << source.indices.size() <<
            ", instances=" << source.instances.size() <<
            ", draw_instances=" << source.draw_instances.size() << ".\n";
        DonutSceneGPUData result = DonutSceneGPUBuilder::build(
            source,
            device,
            command_list,
            used_upload_heaps,
            flush_uploads,
            load_textures);
        util::Logger::g_logger <<
            "Jungle GPU upload complete: Donut vertex_bytes=" <<
            result.vertex_layout.byte_size <<
            ", indices=" << result.index_count <<
            ", instances=" << result.instance_data.size() <<
            ", draw_instances=" << result.draw_instance_data.size() <<
            ", geometry_instances=" <<
            result.geometry_instance_data.size() << ".\n";
        return result;
    }

} // namespace scene
