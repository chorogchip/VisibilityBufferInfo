#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>

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
            DONUT_SUBMESH_BUFFER = 26,
            DONUT_MATERIAL_BUFFER = 27,
            DONUT_TONEMAP_SOURCE = 28,
            DONUT_TONEMAP_EXPOSURE = 29,
            DONUT_TONEMAP_COLOR_LUT = 30,
            DONUT_VISIBILITY_BUFFER = 31,
            DONUT_GBUFFER_UAV_0 = 32,
            DONUT_GBUFFER_UAV_1 = 33,
            DONUT_GBUFFER_UAV_2 = 34,
            DONUT_GBUFFER_UAV_3 = 35,
            DONUT_MATERIAL_TEXTURE_BEGIN = 64,

            BENCH_VISIBILITY_BUFFER = 48,
            BENCH_VERTEX_BUFFER = 49,
            BENCH_INDEX_BUFFER = 50,
            BENCH_MESH_BUFFER = 51,
            BENCH_INSTANCE_BUFFER = 52,
            BENCH_DRAW_INSTANCE_BUFFER = 53,
            BENCH_MATERIAL_BUFFER = 54,
            BENCH_GBUFFER_0 = 55,
            BENCH_GBUFFER_1 = 56,
            BENCH_GBUFFER_2 = 57,
            BENCH_GBUFFER_3 = 58,
            BENCH_GBUFFER_4 = 59,
            BENCH_GBUFFER_5 = 60,
            BENCH_GBUFFER_6 = 61,
            BENCH_GBUFFER_7 = 62,
            BENCH_MATERIAL_TEXTURE_BEGIN = 64,

            COUNT =
            DONUT_MATERIAL_TEXTURE_BEGIN > BENCH_MATERIAL_TEXTURE_BEGIN ?
            DONUT_MATERIAL_TEXTURE_BEGIN : BENCH_MATERIAL_TEXTURE_BEGIN,
        };

        void init(ID3D12Device* device, UINT descriptor_count);

        void create_srv(
            ID3D12Resource* resource,
            const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
            EnumDescPos position,
            UINT offset = 0);

        void create_uav(
            ID3D12Resource* resource,
            const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc,
            EnumDescPos position,
            UINT offset = 0);

        void create_uav_at_cpu(
            ID3D12Resource* resource,
            const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc,
            EnumDescPos position,
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

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_notshader_adr(EnumDescPos position, UINT offset = 0) const {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_cpu_->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(static_cast<UINT>(position) + offset) * descriptor_size_;
            return handle;
        }

    private:
        struct DescriptorRecord {
            bool is_initialized = false;
            bool is_uav = false;
            ID3D12Resource* resource = nullptr;
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        };

        ID3D12Device* device_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_cpu_;
        UINT descriptor_size_ = 0;
        UINT descriptor_count_ = 0;

        std::vector<DescriptorRecord> descriptor_records_;
    };
}
