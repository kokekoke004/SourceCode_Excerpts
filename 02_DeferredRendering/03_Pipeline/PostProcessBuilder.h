#pragma once

#include "IPipelineBuilder.h"

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgiformat.h>
#include <d3dcommon.h> 
#include "d3dx12.h"  
#include<array>
#include "helper.h"
#include"PipelinePsoOptions.h"

namespace Engine::Render::Pipeline
{
    class IPipelineRegistry;
}

namespace Engine::Render::Lighting
{
    struct LightDesc;
}

namespace Engine::Render::Pipeline::Builders
{
    class PostProcessBuilder final : public IPipelineBuilder
    {
    public:
        PostProcessBuilder();

        //ここで作成するパイプラインのセットIDを返却するようにしている
        PipelineSetId GetPipelineSetId() const override { return PipelineSetId::PostProcess; }
        bool Build(const PipelineBuildContext& ctx, class IPipelineRegistry& out) override;

    private:
        struct PostProcessRootParams
        {
            //ranges（全部ここに持つ）
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> cbv{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> srv0{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> srv1{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> srv2{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> srv3{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> lightDepth{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> ssao{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> deferred{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> primitive{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> lighting{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> afterLightingColor{};
            std::array<CD3DX12_DESCRIPTOR_RANGE1, 1> afterLightingHighLum{};
            //root params（rangesを指す）
            std::array<CD3DX12_ROOT_PARAMETER1, 12> params{};
        };


    private:
        ComPtr<ID3D12RootSignature> CreatePostProcessRootSignature(
            ID3D12Device* device,
            const PostProcessRootParams& rp,
            const D3D12_STATIC_SAMPLER_DESC* samplers,
            UINT numSamplers);

        ComPtr<ID3D12PipelineState> CreateSsaoGeneratePso(
            const PipelineBuildContext& ctx,
            ID3D12RootSignature* rootSig,
            ID3DBlob* vs,
            ID3DBlob* ps,
            const RTVFormatOptions& rtvFormatOption);

        D3D12_INPUT_LAYOUT_DESC MakePostProcessInputLayout();

        ComPtr<ID3D12PipelineState> CreatePostProcessPso(
            const PipelineBuildContext& ctx,
            ID3D12RootSignature* rootSig,
            ID3DBlob* vs,
            ID3DBlob* ps,
            const D3D12_BLEND_DESC& blendDesc,
            const PsoOptions& option,
            const RTVFormatOptions& rtvFormatOption);

        PostProcessRootParams MakePostProcessRootParams();

    private:
        //描画する時の頂点バッファの形式設定
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements_;

    };
}
