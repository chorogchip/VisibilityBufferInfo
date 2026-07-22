#include "render/pass/donut/PassDonutTonemap.h"

namespace rndr {

	namespace {

		enum class TonemapRootParam : UINT {
			CONSTANT_BUFFER,
			SOURCE_EXPOSURE_LUT,
			COLOR_LUT_SAMPLER,
		};
	}

    void PassDonutTonemap::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutTonemapResources& resources) {


		for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
			tonemap_constants_[i].init(device_.Get());
			tonemap_constant_resources_[i].init(
				tonemap_constants_[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		resource_manager_frame_.create_rtv(
			eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0,
			render_targets_[0].get());
		resource_manager_frame_.create_rtv(
			eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1,
			render_targets_[1].get());

		resource_manager_shader_.create_srv(
			hdr_color_buffer_.get(),
			eng::ResourceViewBuilder::build_srv(
				hdr_color_buffer_.get(),
				eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D,
				DXGI_FORMAT_R16G16B16A16_FLOAT),
			eng::ResourceManagerShader::EnumDescPos::DONUT_TONEMAP_SOURCE);

		D3D12_SHADER_RESOURCE_VIEW_DESC exposure_srv{};
		exposure_srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		exposure_srv.Format = DXGI_FORMAT_R32_UINT;
		exposure_srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		exposure_srv.Buffer.FirstElement = 0;
		exposure_srv.Buffer.NumElements = 1;
		exposure_srv.Buffer.StructureByteStride = 0;
		exposure_srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		resource_manager_shader_.create_srv(
			exposure_buffer_.get(),
			exposure_srv,
			eng::ResourceManagerShader::EnumDescPos::DONUT_TONEMAP_EXPOSURE);
		resource_manager_shader_.create_srv(
			fallback_color_lut_.get(),
			eng::ResourceViewBuilder::build_srv(
				fallback_color_lut_.get(),
				eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D),
			eng::ResourceManagerShader::EnumDescPos::DONUT_TONEMAP_COLOR_LUT);

		resource_manager_sampler_.create_sampler(
			eng::ResourceManagerSampler::EnumDescPos::DONUT_BRDF,
			eng::ResourceManagerSampler::EnumSamplerType::LINEAR_CLAMP);

		auto vs = dxutl::compile_shader(
			L"assets/shaders/deferred_lighting_VS2.hlsl",
			"vs_5_0", "main", program_argument_);
		auto ps = dxutl::compile_shader(
			L"assets/shaders/donut/donut_tonemapping_PS.hlsl",
			"ps_5_1", "main", program_argument_);

		tonemap_pso_.init(device_.Get());
		tonemap_pso_.set_graphics();
		auto root_signature = eng::RootSignatureBuilder{}
			.root_cbv().reg(0).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
			.srv_tabl().reg(0).cnt(3).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
			.spl_tabl().reg(0).cnt(1).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
			.build(device_.Get());
		tonemap_pso_.set_root_signature(root_signature.Get());
		tonemap_pso_.set_shader_vertex(vs.Get());
		tonemap_pso_.set_shader_pixel(ps.Get());
		tonemap_pso_.set_fullscreen();
		tonemap_pso_.set_render_targets(1, DXGI_FORMAT_R8G8B8A8_UNORM);
		tonemap_pso_.build();




		tonemap_constants_[frame_index_].buffer = DonutToneMappingConstants{};
		tonemap_constants_[frame_index_].buffer.view_size[0] = width_;
		tonemap_constants_[frame_index_].buffer.view_size[1] = height_;
		tonemap_constants_[frame_index_].buffer.min_adapted_luminance = 1.0f;
		tonemap_constants_[frame_index_].buffer.max_adapted_luminance = 1.0f;
		tonemap_constants_[frame_index_].buffer.exposure_scale = 1.0f;
		tonemap_constants_[frame_index_].buffer.white_point_inv_squared = 1.0f;
		tonemap_constants_[frame_index_].update();
    }

    void PassDonutTonemap::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

		hdr_color_buffer_.transition(
			command_list_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		render_targets_[frame_index_].transition(
			command_list_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

		command_list_->SetPipelineState(tonemap_pso_.get());
		command_list_->SetGraphicsRootSignature(tonemap_pso_.get_root_signature());
		ID3D12DescriptorHeap* heaps[] = {
			resource_manager_shader_.get(),
			resource_manager_sampler_.get()
		};
		command_list_->SetDescriptorHeaps(_countof(heaps), heaps);
		command_list_->SetGraphicsRootConstantBufferView(
			static_cast<UINT>(TonemapRootParam::CONSTANT_BUFFER),
			tonemap_constant_resources_[frame_index_].get()->GetGPUVirtualAddress());
		command_list_->SetGraphicsRootDescriptorTable(
			static_cast<UINT>(TonemapRootParam::SOURCE_EXPOSURE_LUT),
			resource_manager_shader_.get_gpu_adr(
				eng::ResourceManagerShader::EnumDescPos::DONUT_TONEMAP_SOURCE));
		command_list_->SetGraphicsRootDescriptorTable(
			static_cast<UINT>(TonemapRootParam::COLOR_LUT_SAMPLER),
			resource_manager_sampler_.get_gpu_adr(
				eng::ResourceManagerSampler::EnumDescPos::DONUT_BRDF));

		command_list_->RSSetViewports(1, &viewport_);
		command_list_->RSSetScissorRects(1, &scissor_rect_);

		const auto rtv = resource_manager_frame_.get_rtv(
			eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0, frame_index_);
		command_list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		command_list_->ClearRenderTargetView(
			rtv, util::RenderConstants::CLEAR_COLOR, 0, nullptr);
		command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list_->DrawInstanced(3, 1, 0, 0);
    }
}