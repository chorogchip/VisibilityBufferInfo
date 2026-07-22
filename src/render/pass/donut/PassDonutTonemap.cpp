#include "render/pass/donut/PassDonutTonemap.h"

#include "util/RenderConstants.h"
#include "dx_util/ShaderUtils.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"

namespace rndr {

	namespace {

		enum class RootParam : UINT {
			CONSTANT_BUFFER,
			SOURCE_EXPOSURE_LUT,
			COLOR_LUT_SAMPLER,
		};
	}

	void PassDonutTonemap::init(
		ID3D12Device* device,
		const util::ProgramArgument& arguments,
		const PassDonutTonemapResources& resources) {

		resources_ = resources;

		for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
			tonemap_constants_[i].init(device);
			tonemap_constant_resources_[i].init(
				tonemap_constants_[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		}
		fallback_color_lut_.init(
			dxutl::create_texture2d(
				device, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

		resources_.frame_manager->create_rtv(
			eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0,
			resources_.back_buffers[0]->get());
		resources_.frame_manager->create_rtv(
			eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1,
			resources_.back_buffers[1]->get());

		resources_.shader_manager->create_srv(
			resources_.hdr_color_buffer->get(),
			eng::ResourceViewBuilder::build_srv(
				resources_.hdr_color_buffer->get(),
				eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D,
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

		resources_.shader_manager->create_srv(
			resources_.exposure_buffer->get(),
			exposure_srv,
			eng::ResourceManagerShader::EnumDescPos::DONUT_TONEMAP_EXPOSURE);
		resources_.shader_manager->create_srv(
			fallback_color_lut_.get(),
			eng::ResourceViewBuilder::build_srv(
				fallback_color_lut_.get(),
				eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D),
			eng::ResourceManagerShader::EnumDescPos::DONUT_TONEMAP_COLOR_LUT);

		resources_.sampler_manager->create_sampler(
			eng::ResourceManagerSampler::EnumDescPos::DONUT_BRDF,
			eng::ResourceManagerSampler::EnumSamplerType::LINEAR_CLAMP);

		auto vs = dxutl::compile_shader(
			L"assets/shaders/deferred_lighting_VS2.hlsl",
			"vs_5_0", "main", arguments);
		auto ps = dxutl::compile_shader(
			L"assets/shaders/donut/donut_tonemapping_PS.hlsl",
			"ps_5_1", "main", arguments);

		pso_.init(device);
		pso_.set_graphics();
		auto root_signature = eng::RootSignatureBuilder{}
			.root_cbv().reg(0).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
			.srv_tabl().reg(0).cnt(3).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
			.spl_tabl().reg(0).cnt(1).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
			.build(device);
		pso_.set_root_signature(root_signature.Get());
		pso_.set_shader_vertex(vs.Get());
		pso_.set_shader_pixel(ps.Get());
		pso_.set_fullscreen();
		pso_.set_render_targets(1, DXGI_FORMAT_R8G8B8A8_UNORM);
		pso_.build();

		
		for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
			tonemap_constants_[i].buffer = DonutToneMappingConstants{};
			tonemap_constants_[i].buffer.view_size[0] = arguments.window_width;
			tonemap_constants_[i].buffer.view_size[1] = arguments.window_height;
			tonemap_constants_[i].buffer.min_adapted_luminance = 1.0f;
			tonemap_constants_[i].buffer.max_adapted_luminance = 1.0f;
			tonemap_constants_[i].buffer.exposure_scale = 1.0f;
			tonemap_constants_[i].buffer.white_point_inv_squared = 1.0f;
			tonemap_constants_[i].update();
		}
	}

	void PassDonutTonemap::render(
		ID3D12GraphicsCommandList* command_list,
		UINT frame_index,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor_rect) {

		resources_.hdr_color_buffer->transition(
			command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		resources_.back_buffers[frame_index]->transition(
			command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

		command_list->SetPipelineState(pso_.get());
		command_list->SetGraphicsRootSignature(pso_.get_root_signature());
		ID3D12DescriptorHeap* heaps[] = {
			resources_.shader_manager->get(),
			resources_.sampler_manager->get()
		};
		command_list->SetDescriptorHeaps(_countof(heaps), heaps);
		command_list->SetGraphicsRootConstantBufferView(
			static_cast<UINT>(RootParam::CONSTANT_BUFFER),
			tonemap_constant_resources_[frame_index].get()->GetGPUVirtualAddress());
		command_list->SetGraphicsRootDescriptorTable(
			static_cast<UINT>(RootParam::SOURCE_EXPOSURE_LUT),
			resources_.shader_manager->get_gpu_adr(
				eng::ResourceManagerShader::EnumDescPos::DONUT_TONEMAP_SOURCE));
		command_list->SetGraphicsRootDescriptorTable(
			static_cast<UINT>(RootParam::COLOR_LUT_SAMPLER),
			resources_.sampler_manager->get_gpu_adr(
				eng::ResourceManagerSampler::EnumDescPos::DONUT_BRDF));

		command_list->RSSetViewports(1, &viewport);
		command_list->RSSetScissorRects(1, &scissor_rect);

		const auto rtv = resources_.frame_manager->get_rtv(
			eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0, frame_index);
		command_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		command_list->ClearRenderTargetView(
			rtv, util::RenderConstants::CLEAR_COLOR, 0, nullptr);
		command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list->DrawInstanced(3, 1, 0, 0);
	}
}
