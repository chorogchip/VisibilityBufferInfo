#include "engine/RootSignatureBuilder.h"

#include <string>

#include <d3dcompiler.h>

#include "util/Logger.h"

namespace eng {

    RootSignatureBuilder::ParameterProxy::ParameterProxy(
        RootSignatureBuilder& owner,
        D3D12_ROOT_PARAMETER_TYPE root_type,
        D3D12_DESCRIPTOR_RANGE_TYPE range_type)
        : owner_(owner), root_type_(root_type), range_type_(range_type) {}

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::reg(UINT shader_register) {
        base_shader_register_ = shader_register;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::cnt(UINT count) {
        count_ = count;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::spc(UINT register_space) {
        register_space_ = register_space;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::vis(
            D3D12_SHADER_VISIBILITY visibility) {
        visibility_ = visibility;
        return *this;
    }

    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::vis_vtx() {
        visibility_ = D3D12_SHADER_VISIBILITY_VERTEX;
        return *this;
    }
    RootSignatureBuilder::ParameterProxy&
        RootSignatureBuilder::ParameterProxy::vis_pxl() {
        visibility_ = D3D12_SHADER_VISIBILITY_PIXEL;
        return *this;
    }

    RootSignatureBuilder& RootSignatureBuilder::ParameterProxy::add() {
        if (root_type_ == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
            return owner_.add_constants(
                base_shader_register_, count_, register_space_, visibility_);
        }
        if (root_type_ == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            return owner_.add_descriptor_table(
                range_type_, base_shader_register_, count_, register_space_, visibility_);
        }
        util::Logger::g_logger.assert_with_log(
            count_ == 0, "root descriptors do not accept cnt()");
        return owner_.add_root_descriptor(
            root_type_, base_shader_register_, register_space_, visibility_);
    }

    RootSignatureBuilder& RootSignatureBuilder::reset() {
        flags_ = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        parameters_.clear();
        return *this;
    }

    RootSignatureBuilder& RootSignatureBuilder::set_flags(
        D3D12_ROOT_SIGNATURE_FLAGS flags) {
        flags_ = flags;
        return *this;
    }

    RootSignatureBuilder& RootSignatureBuilder::add_constants(
        UINT shader_register,
        UINT value_count,
        UINT register_space,
        D3D12_SHADER_VISIBILITY visibility) {
        util::Logger::g_logger.assert_with_log(
            value_count > 0, "root constants require at least one value");

        ParameterDesc desc{};
        desc.kind = ParameterKind::CONSTANTS;
        desc.root_type = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        desc.shader_register = shader_register;
        desc.register_space = register_space;
        desc.value_or_descriptor_count = value_count;
        desc.visibility = visibility;
        parameters_.push_back(desc);
        return *this;
    }

    RootSignatureBuilder::ParameterProxy RootSignatureBuilder::constant() {
        return ParameterProxy(*this, D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS);
    }

    RootSignatureBuilder::ParameterProxy RootSignatureBuilder::root_cbv() {
        return ParameterProxy(*this, D3D12_ROOT_PARAMETER_TYPE_CBV);
    }

    RootSignatureBuilder::ParameterProxy RootSignatureBuilder::root_srv() {
        return ParameterProxy(*this, D3D12_ROOT_PARAMETER_TYPE_SRV);
    }

    RootSignatureBuilder::ParameterProxy RootSignatureBuilder::root_uav() {
        return ParameterProxy(*this, D3D12_ROOT_PARAMETER_TYPE_UAV);
    }

    RootSignatureBuilder::ParameterProxy RootSignatureBuilder::srv_tabl() {
        return ParameterProxy(
            *this,
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
    }

    RootSignatureBuilder::ParameterProxy RootSignatureBuilder::uav_tabl() {
        return ParameterProxy(
            *this,
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV);
    }

    RootSignatureBuilder::ParameterProxy RootSignatureBuilder::spl_tabl() {
        return ParameterProxy(
            *this,
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);
    }

    RootSignatureBuilder& RootSignatureBuilder::add_root_descriptor(
        D3D12_ROOT_PARAMETER_TYPE type,
        UINT shader_register,
        UINT register_space,
        D3D12_SHADER_VISIBILITY visibility) {
        ParameterDesc desc{};
        desc.kind = ParameterKind::ROOT_DESCRIPTOR;
        desc.root_type = type;
        desc.shader_register = shader_register;
        desc.register_space = register_space;
        desc.visibility = visibility;
        parameters_.push_back(desc);
        return *this;
    }

    RootSignatureBuilder& RootSignatureBuilder::add_descriptor_table(
        D3D12_DESCRIPTOR_RANGE_TYPE type,
        UINT base_shader_register,
        UINT descriptor_count,
        UINT register_space,
        D3D12_SHADER_VISIBILITY visibility) {
        util::Logger::g_logger.assert_with_log(
            descriptor_count > 0,
            "descriptor table requires at least one descriptor");

        ParameterDesc desc{};
        desc.kind = ParameterKind::DESCRIPTOR_TABLE;
        desc.root_type = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        desc.range_type = type;
        desc.shader_register = base_shader_register;
        desc.register_space = register_space;
        desc.value_or_descriptor_count = descriptor_count;
        desc.visibility = visibility;
        parameters_.push_back(desc);
        return *this;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignatureBuilder::build(
        ID3D12Device* device) const {
        util::Logger::g_logger.assert_with_log(
            device != nullptr, "root signature builder requires a device");

        size_t table_count = 0;
        for (const ParameterDesc& desc : parameters_) {
            if (desc.kind == ParameterKind::DESCRIPTOR_TABLE)
                ++table_count;
        }

        std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
        ranges.reserve(table_count);
        std::vector<D3D12_ROOT_PARAMETER> parameters(parameters_.size());

        for (size_t i = 0; i < parameters_.size(); ++i) {
            const ParameterDesc& source = parameters_[i];
            D3D12_ROOT_PARAMETER& destination = parameters[i];
            destination.ParameterType = source.root_type;
            destination.ShaderVisibility = source.visibility;

            if (source.kind == ParameterKind::CONSTANTS) {
                destination.Constants.ShaderRegister = source.shader_register;
                destination.Constants.RegisterSpace = source.register_space;
                destination.Constants.Num32BitValues = source.value_or_descriptor_count;
            }
            else if (source.kind == ParameterKind::ROOT_DESCRIPTOR) {
                destination.Descriptor.ShaderRegister = source.shader_register;
                destination.Descriptor.RegisterSpace = source.register_space;
            }
            else {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = source.range_type;
                range.NumDescriptors = source.value_or_descriptor_count;
                range.BaseShaderRegister = source.shader_register;
                range.RegisterSpace = source.register_space;
                range.OffsetInDescriptorsFromTableStart =
                    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                ranges.push_back(range);

                destination.DescriptorTable.NumDescriptorRanges = 1;
                destination.DescriptorTable.pDescriptorRanges = &ranges.back();
            }
        }

        D3D12_ROOT_SIGNATURE_DESC root_desc{};
        root_desc.NumParameters = static_cast<UINT>(parameters.size());
        root_desc.pParameters = parameters.data();
        root_desc.Flags = flags_;

        Microsoft::WRL::ComPtr<ID3DBlob> serialized;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        const HRESULT serialize_result = D3D12SerializeRootSignature(
            &root_desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            serialized.ReleaseAndGetAddressOf(),
            errors.ReleaseAndGetAddressOf());
        if (FAILED(serialize_result)) {
            std::string message = "serialize root signature";
            if (errors) {
                message += ": ";
                message.append(
                    static_cast<const char*>(errors->GetBufferPointer()),
                    errors->GetBufferSize());
            }
            util::Logger::g_logger.assert_with_log(false, message.c_str());
        }

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        const HRESULT create_result = device->CreateRootSignature(
            0,
            serialized->GetBufferPointer(),
            serialized->GetBufferSize(),
            IID_PPV_ARGS(root_signature.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(
            SUCCEEDED(create_result), "create root signature");
        return root_signature;
    }

}
