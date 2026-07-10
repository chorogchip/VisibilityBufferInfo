#include "RendererBase.h"

#include <algorithm>
#include <numeric>
#include <iostream>
#include <cstring>

#include "util/Utils.h"
#include "dx_util/DescriptorUtils.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/DX12UtilsStruct.h"

#include "scene/SceneLoader.h"
#include "scene/SceneResourceBuilder.h"

#include "util/Macros.h"

void RendererBase::init(const util::ProgramArgument& args, HWND hwnd) {

    this->rndr::ProcedureBase::init(args);

    dx12_context_ = dxutl::DX12Context::create_context(
        hwnd, args.window_width, args.window_height,
        DXGI_FORMAT_R8G8B8A8_UNORM, FRAME_COUNT);

    for (int i = 0; i < FRAME_COUNT; ++i) {
        dx12_frame_contexts_[i] = dxutl::DX12FrameContext::create_frame_context(
            dx12_context_.device.Get()
        );
    }

    dx12_list_ = dxutl::DX12List::create_list(
        dx12_context_.device.Get(),
        dx12_frame_contexts_[0].command_allocator.Get()
    );

    auto tmp_list = dxutl::DX12List::create_list(
        dx12_context_.device.Get(),
        dx12_frame_contexts_[0].command_allocator.Get()
    );

    dx12_fence_ = dxutl::DX12Fence::create_fence(
        dx12_context_.device.Get(),
        dx12_context_.queue.Get()
    );

    dx12_query_buf_ = dxutl::DX12QueryBuf::create_query_buf(
        dx12_context_.device.Get(),
        dx12_context_.queue.Get(),
        FRAME_COUNT * 4 * 2
    );

    viewport_ = dxutl::create_viewport(
        static_cast<float>(program_arguments_.window_width),
        static_cast<float>(program_arguments_.window_height)
    );

    scissor_rect_ = dxutl::create_scissor_rect(
        static_cast<LONG>(program_arguments_.window_width),
        static_cast<LONG>(program_arguments_.window_height)
    );

    this->init_();

    this->create_meshbuffers();
    this->create_dummy_textures();
    this->create_constbuffers();

    this->create_depth_stencil_resources();
    this->create_render_target_views();
    this->create_shader_visible_srv_heap();
    this->create_shader_resources();

    this->create_root_signature();
    this->create_pso();
}

void RendererBase::render() {
    this->render_();
    this->move_to_next_frame();
}

void RendererBase::close() {
    dx12_fence_.wait_for_gpu();
}

UINT RendererBase::dsv_descriptor_count() const { return 1; }

D3D12_RESOURCE_STATES RendererBase::depth_stencil_initial_state() const {
    return D3D12_RESOURCE_STATE_DEPTH_WRITE;
}
void RendererBase::create_extra_depth_stencil_views() {}

UINT RendererBase::rtv_descriptor_count() const {
    return FRAME_COUNT;
}

void RendererBase::create_extra_render_target_views(D3D12_CPU_DESCRIPTOR_HANDLE) {}

UINT RendererBase::srv_descriptor_count() const {
    return 0;
}

void RendererBase::create_shader_resources() {}

void RendererBase::create_depth_stencil_resources() {
    dsv_heap_ = dxutl::create_descriptor_heap(
        dx12_context_.device.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        dsv_descriptor_count(),
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    dsv_descriptor_size_ = dxutl::descriptor_size(dx12_context_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    depth_stencil_buffer_ = dxutl::create_depth_stencil_buffer(
        dx12_context_.device.Get(),
        program_arguments_.window_width,
        program_arguments_.window_height,
        DEPTH_STENCIL_FORMAT_,
        depth_stencil_initial_state());

    dxutl::create_depth_stencil_view(
        dx12_context_.device.Get(),
        depth_stencil_buffer_.Get(),
        DEPTH_STENCIL_FORMAT_,
        dsv_heap_->GetCPUDescriptorHandleForHeapStart());

    create_extra_depth_stencil_views();
}

void RendererBase::create_render_target_views() {
    rtv_heap_ = dxutl::create_descriptor_heap(
        dx12_context_.device.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        rtv_descriptor_count(),
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    rtv_descriptor_size_ = dxutl::descriptor_size(dx12_context_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    dxutl::create_swapchain_render_target_views(
        dx12_context_.device.Get(), dx12_context_.swap_chain.Get(), rtv_heap_.Get(), rtv_descriptor_size_, FRAME_COUNT, render_targets_);

    create_extra_render_target_views(dxutl::offset_cpu_descriptor(
        rtv_heap_->GetCPUDescriptorHandleForHeapStart(), rtv_descriptor_size_, FRAME_COUNT));
}

void RendererBase::create_shader_visible_srv_heap() {
    const UINT descriptor_count = srv_descriptor_count();
    if (descriptor_count == 0) return;

    srv_heap_ = dxutl::create_descriptor_heap(
        dx12_context_.device.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        descriptor_count,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        "create srv descriptor heap");
    srv_descriptor_size_ = dxutl::descriptor_size(dx12_context_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void RendererBase::create_meshbuffers() {
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;

    dx12_list_.reset(
        dx12_frame_contexts_[dx12_context_.frame_index].command_allocator.Get(),
        pipeline_state_.Get());

    scene_gpu_ = scene::SceneResourceBuilder::build(
        *scene_cpu_, dx12_context_.device.Get(), dx12_list_.command_list.Get(), used_upload_heaps);

    dx12_list_.close();
    dx12_list_.execute(dx12_context_.queue.Get());
    dx12_fence_.wait_for_gpu();
}

void RendererBase::create_dummy_textures() {
    const UINT texture_count = std::max(1u, program_arguments_.texture_count);
    const UINT texture_size = std::max(1u, program_arguments_.texture_size);
    const DXGI_FORMAT texture_format = DXGI_FORMAT_R8G8B8A8_UNORM;

    dummy_textures_.clear();
    dummy_textures_.resize(texture_count);

    D3D12_RESOURCE_DESC texture_desc{};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Alignment = 0;
    texture_desc.Width = texture_size;
    texture_desc.Height = texture_size;
    texture_desc.DepthOrArraySize = 1;
    texture_desc.MipLevels = 1;
    texture_desc.Format = texture_format;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    UINT64 upload_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT row_count = 0;
    UINT64 row_size_in_bytes = 0;
    dx12_context_.device->GetCopyableFootprints(
        &texture_desc, 0, 1, 0, &footprint, &row_count, &row_size_in_bytes, &upload_size);
    (void)row_count;
    (void)row_size_in_bytes;

    std::vector<ComPtr<ID3D12Resource>> upload_buffers;
    upload_buffers.resize(texture_count);

    dx12_list_.reset(
        dx12_frame_contexts_[dx12_context_.frame_index].command_allocator.Get(),
        pipeline_state_.Get());

    for (UINT texture_index = 0; texture_index < texture_count; ++texture_index) {
        dummy_textures_[texture_index] = dxutl::create_committed_resource(
            dx12_context_.device.Get(),
            texture_desc,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COPY_DEST);

        upload_buffers[texture_index] = dxutl::create_buffer(dx12_context_.device.Get(), upload_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        void* mapped_data = dxutl::map_upload_buffer(upload_buffers[texture_index].Get());
        auto* dst = static_cast<uint8_t*>(mapped_data);
        for (UINT y = 0; y < texture_size; ++y) {
            auto* row = dst + footprint.Offset + static_cast<size_t>(y) * footprint.Footprint.RowPitch;
            for (UINT x = 0; x < texture_size; ++x) {
                const size_t texel_offset = static_cast<size_t>(x) * 4;
                row[texel_offset + 0] = static_cast<uint8_t>((x * 17 + texture_index * 29) & 0xff);
                row[texel_offset + 1] = static_cast<uint8_t>((y * 19 + texture_index * 31) & 0xff);
                row[texel_offset + 2] = static_cast<uint8_t>(((x + y) * 13 + texture_index * 37) & 0xff);
                row[texel_offset + 3] = 255;
            }
        }
        upload_buffers[texture_index]->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION dst_location{};
        dst_location.pResource = dummy_textures_[texture_index].Get();
        dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_location.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src_location{};
        src_location.pResource = upload_buffers[texture_index].Get();
        src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_location.PlacedFootprint = footprint;

        dx12_list_.command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
        dxutl::transition_resource(dx12_list_.command_list.Get(), dummy_textures_[texture_index].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    dx12_list_.close();
    dx12_list_.execute(dx12_context_.queue.Get());
    dx12_fence_.wait_for_gpu();
}

void RendererBase::create_constbuffers() {
    camera_.set_pos(program_arguments_.camera_pos_x, program_arguments_.camera_pos_y, program_arguments_.camera_pos_z);
    camera_.lookat(program_arguments_.camera_lookat_x, program_arguments_.camera_lookat_y, program_arguments_.camera_lookat_z);
    camera_.set_fovy_nearz_farz(program_arguments_.camera_fov, program_arguments_.camera_near_z,
        program_arguments_.camera_far_z);

    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_,
        DirectX::XMMatrixTranspose(camera_.get_mat_view()));
    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_proj_,
        DirectX::XMMatrixTranspose(camera_.get_mat_proj(program_arguments_.window_width, program_arguments_.window_height)));
    matrix_buf_cpu_.viewport_size_ = DirectX::XMFLOAT2(
        static_cast<float>(program_arguments_.window_width), static_cast<float>(program_arguments_.window_height));
    matrix_buf_cpu_.inv_viewport_size_ = DirectX::XMFLOAT2(
        1.0f / static_cast<float>(program_arguments_.window_width),
        1.0f / static_cast<float>(program_arguments_.window_height));

    constexpr size_t matrix_buf_size_aligned =
        Utils::GetAlignedAddress(sizeof(ConstBufMatrices), 256ULL);

    for (int i = 0; i < FRAME_COUNT; ++i) {
        buf_constant_[i] = dxutl::create_buffer(dx12_context_.device.Get(), matrix_buf_size_aligned, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        buf_constant_mapped_[i] = dxutl::map_upload_buffer(buf_constant_[i].Get());

        std::memcpy(buf_constant_mapped_[i], &matrix_buf_cpu_, sizeof(matrix_buf_cpu_));
    }
}

void RendererBase::copy_camera_data() {
    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_,
        DirectX::XMMatrixTranspose(camera_.get_mat_view()));
    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_proj_,
        DirectX::XMMatrixTranspose(camera_.get_mat_proj(program_arguments_.window_width, program_arguments_.window_height)));
    matrix_buf_cpu_.viewport_size_ = DirectX::XMFLOAT2(static_cast<float>(program_arguments_.window_width), static_cast<float>(program_arguments_.window_height));
    matrix_buf_cpu_.inv_viewport_size_ = DirectX::XMFLOAT2(1.0f / static_cast<float>(program_arguments_.window_width), 1.0f / static_cast<float>(program_arguments_.window_height));
    memcpy(buf_constant_mapped_[dx12_context_.frame_index], &matrix_buf_cpu_, sizeof(matrix_buf_cpu_));
}

void RendererBase::create_texture_srv_descriptors(D3D12_CPU_DESCRIPTOR_HANDLE srv_handle) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    for (UINT i = 0; i < program_arguments_.texture_count; ++i) {
        dx12_context_.device->CreateShaderResourceView(dummy_textures_[i].Get(), &srv_desc, srv_handle);
        srv_handle.ptr += dx12_context_.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
}

void RendererBase::move_to_next_frame() {
    const UINT finished_frame_index = dx12_context_.frame_index;
    this->curr_context().fence_value = dx12_fence_.signal();
    dx12_context_.frame_index = dx12_context_.swap_chain->GetCurrentBackBufferIndex();
    dx12_fence_.wait_for_value(this->curr_context().fence_value);
    /*
    std::vector<double> passes = frame_time_.read_timestamp(frame_index_);
    passes.push_back(std::accumulate(passes.begin(), passes.end(), 0.0));
    for (int i = 0; i < passes.size(); ++i) frame_counter_.record(i, passes[i]);*/
    frame_counter_.next_tick();
}

dxutl::DX12FrameContext& RendererBase::curr_context() {
    return dx12_frame_contexts_[dx12_context_.frame_index];
}