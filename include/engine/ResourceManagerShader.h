#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace eng {

    class ResourceManagerShader {
    public:
        enum class EnumDescPos : UINT {
            DONUT_SHADOW_MAP_ARRAY = 0,
            DONUT_DIFFUSE_LIGHT_PROBE = 1,
            DONUT_SPECULAR_LIGHT_PROBE = 2,
            DONUT_ENVIRONMENT_BRDF = 3,
            DONUT_GBUFFER_DEPTH = 8,
            DONUT_GBUFFER_0 = 9,
            DONUT_GBUFFER_1 = 10,
            DONUT_GBUFFER_2 = 11,
            DONUT_GBUFFER_3 = 12,
            DONUT_INDIRECT_DIFFUSE = 14,
            DONUT_INDIRECT_SPECULAR = 15,
            DONUT_SHADOW_CHANNELS = 16,
            DONUT_AMBIENT_OCCLUSION = 17,
            DONUT_INSTANCE_BUFFER = 18,
            DONUT_VERTEX_BUFFER = 19,
            DONUT_POST_PROCESS_SOURCE = 20,
            DONUT_MOTION_VECTORS = 21,
            DONUT_TAA_FEEDBACK = 22,
            DONUT_EXPOSURE = 23,
            DONUT_COLOR_LUT = 24,
            DONUT_HDR_COLOR_UAV = 25,

            BENCH_VISIBILITY_BUFFER = 32,
            BENCH_VERTEX_BUFFER = 33,
            BENCH_INDEX_BUFFER = 34,
            BENCH_MESH_BUFFER = 35,
            BENCH_INSTANCE_BUFFER = 36,
            BENCH_MATERIAL_BUFFER = 37,
            BENCH_GBUFFER_0 = 38,
            BENCH_GBUFFER_1 = 39,
            BENCH_GBUFFER_2 = 40,
            BENCH_GBUFFER_3 = 41,
            BENCH_GBUFFER_4 = 42,
            BENCH_GBUFFER_5 = 43,
            BENCH_GBUFFER_6 = 44,
            BENCH_GBUFFER_7 = 45,
            BENCH_MATERIAL_TEXTURE_BEGIN = 46,
            COUNT = BENCH_MATERIAL_TEXTURE_BEGIN
        };

        void init(ID3D12Device* device, UINT descriptor_count);
        void create_srv(
            EnumDescPos position,
            ID3D12Resource* resource,
            const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr,
            UINT offset = 0);
        void create_uav(
            EnumDescPos position,
            ID3D12Resource* resource,
            const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr,
            UINT offset = 0);

        [[nodiscard]] ID3D12DescriptorHeap* get() const { return heap_.Get(); }
        [[nodiscard]] UINT get_size() const { return descriptor_size_; }
        [[nodiscard]] UINT get_count() const { return descriptor_count_; }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_adr(EnumDescPos position, UINT offset = 0) const {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(static_cast<UINT>(position) + offset) * descriptor_size_;
            return handle;
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_adr(EnumDescPos position, UINT offset = 0) const {
            D3D12_GPU_DESCRIPTOR_HANDLE handle = heap_->GetGPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<UINT64>(static_cast<UINT>(position) + offset) * descriptor_size_;
            return handle;
        }

    private:
        ID3D12Device* device_ = nullptr;
        UINT descriptor_size_ = 0;
        UINT descriptor_count_ = 0;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    };

}
