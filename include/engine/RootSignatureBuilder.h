#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <vector>

namespace eng {

    class RootSignatureBuilder {
    public:
        class DescriptorTableProxy {
        public:
            DescriptorTableProxy(const DescriptorTableProxy&) = delete;
            DescriptorTableProxy& operator=(const DescriptorTableProxy&) = delete;
            DescriptorTableProxy(DescriptorTableProxy&&) = delete;
            DescriptorTableProxy& operator=(DescriptorTableProxy&&) = delete;

            DescriptorTableProxy& base(UINT shader_register);
            DescriptorTableProxy& cnt(UINT descriptor_count);
            DescriptorTableProxy& spc(UINT register_space);
            DescriptorTableProxy& vis(D3D12_SHADER_VISIBILITY visibility);

            RootSignatureBuilder& add();

        private:
            friend class RootSignatureBuilder;

            DescriptorTableProxy(
                RootSignatureBuilder& owner,
                D3D12_DESCRIPTOR_RANGE_TYPE type);

            RootSignatureBuilder& owner_;
            D3D12_DESCRIPTOR_RANGE_TYPE type_;
            UINT base_shader_register_ = 0;
            UINT descriptor_count_ = 0;
            UINT register_space_ = 0;
            D3D12_SHADER_VISIBILITY visibility_ = D3D12_SHADER_VISIBILITY_ALL;
        };

        RootSignatureBuilder& reset();

        RootSignatureBuilder& set_flags(D3D12_ROOT_SIGNATURE_FLAGS flags);

        RootSignatureBuilder& add_constants(
            UINT shader_register,
            UINT value_count,
            UINT register_space = 0,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        RootSignatureBuilder& add_root_cbv(
            UINT shader_register,
            UINT register_space = 0,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        RootSignatureBuilder& add_root_srv(
            UINT shader_register,
            UINT register_space = 0,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        RootSignatureBuilder& add_root_uav(
            UINT shader_register,
            UINT register_space = 0,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        [[nodiscard]] DescriptorTableProxy srv_table();
        [[nodiscard]] DescriptorTableProxy uav_table();
        [[nodiscard]] DescriptorTableProxy sampler_table();

        [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12RootSignature> build(
            ID3D12Device* device) const;

    private:
        enum class ParameterKind {
            CONSTANTS,
            ROOT_DESCRIPTOR,
            DESCRIPTOR_TABLE
        };

        struct ParameterDesc {
            ParameterKind kind = ParameterKind::CONSTANTS;
            D3D12_ROOT_PARAMETER_TYPE root_type = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            D3D12_DESCRIPTOR_RANGE_TYPE range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            UINT shader_register = 0;
            UINT register_space = 0;
            UINT value_or_descriptor_count = 0;
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
        };

        RootSignatureBuilder& add_root_descriptor(
            D3D12_ROOT_PARAMETER_TYPE type,
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility);

        RootSignatureBuilder& add_descriptor_table(
            D3D12_DESCRIPTOR_RANGE_TYPE type,
            UINT base_shader_register,
            UINT descriptor_count,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility);

        D3D12_ROOT_SIGNATURE_FLAGS flags_ = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        std::vector<ParameterDesc> parameters_;
    };

}
