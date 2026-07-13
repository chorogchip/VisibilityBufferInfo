#include "dx_util/RendererBindingFactory.h"

#include "dx_util/DescriptorUtils.h"

namespace dxutl {

    static void build_srv(
        RenderBindings& ret,
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        util::RenderVariant variant,
        RenderResourceForBindings& bindings);

    static void build_root_signature(
        RenderBindings& ret,
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        util::RenderVariant variant,
        RenderResourceForBindings& bindings);

    RenderBindings build_bindings(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        util::RenderVariant variant,
        RenderResourceForBindings& bindings) {

        RenderBindings ret{};

        build_srv(ret, device, arguments, variant, bindings);
        build_root_signature(ret, device, arguments, variant, bindings);

        return ret;
    }

    static void build_srv(
        RenderBindings& ret,
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        util::RenderVariant variant,
        RenderResourceForBindings& bindings) {

        /*
            srv heap

            dummy textures: all variant
            gbuffers: deferred, deferred-prepass, visbuf-gbuffer
            visbuf resources(visbuf, vertices, indices, meshes, objects, materials): visbuf
        */

        const UINT srv_desc_count_texture = arguments.texture_count;
        const UINT srv_desc_count_gbuffers = variant.do_use_gbuffer() ? arguments.gbuffer_cnt : 0;
        const UINT src_desc_count_buffers = variant.is_visbuf() ? 6 : 1; // 1 for objects

        const UINT srv_desc_offset_texture = 0;
        const UINT srv_desc_offset_gbuffer = srv_desc_offset_texture + srv_desc_count_texture;
        const UINT srv_desc_offset_buffers = srv_desc_offset_gbuffer + srv_desc_count_gbuffers;

        const UINT srv_desc_count = srv_desc_offset_buffers + src_desc_count_buffers;

        ret.srv_heap = dxutl::create_descriptor_heap(
            device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            srv_desc_count,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            "create srv descriptor heap");

        ret.srv_heap_size = srv_desc_count;
        ret.srv_heap_elem_size = device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_CPU_DESCRIPTOR_HANDLE dest_handle =
            ret.srv_heap->GetCPUDescriptorHandleForHeapStart();
        /*
        for (UINT i = 0; i < srv_desc_count_texture; ++i) {

            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = format;
            srv_desc.ViewDimension = dimention;
            srv_desc.Texture2D.MipLevels = 1;

            device->CreateShaderResourceView(
                bindings.textures[i], &srv_desc, dest_handle);

            dest_handle.ptr += ret.srv_heap_elem_size;
        }

        for (UINT i = 0; i < srv_desc_count_gbuffers; ++i) {

            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = format;
            srv_desc.ViewDimension = dimention;
            srv_desc.Texture2D.MipLevels = 1;

            device->CreateShaderResourceView(
                bindings.gbuffers[i], &srv_desc, dest_handle);

            dest_handle.ptr += ret.srv_heap_elem_size;
        }*/
    }

    static void build_root_signature(
        RenderBindings& ret,
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        util::RenderVariant variant,
        RenderResourceForBindings& bindings) {

    }
}
