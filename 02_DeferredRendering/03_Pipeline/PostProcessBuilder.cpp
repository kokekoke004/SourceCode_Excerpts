#include "PostProcessBuilder.h"
#include "pipelineStateObject.h"
#include"GraphicsUtility.h"
#include"ShaderCompileUtility.h"
#include"BlendPresets.h"
#include"IPipelineRegistry.h"

#include <d3dcompiler.h>
#include <stdexcept>
#include "d3dx12.h" 
#include "helper.h"

namespace Engine::Render::Pipeline::Builders
{

	PostProcessBuilder::PostProcessBuilder()
	{

	}


	bool PostProcessBuilder::Build(const PipelineBuildContext& ctx, class IPipelineRegistry& out)
	{
		auto rootParam = MakePostProcessRootParams();
		auto blendDesc = Presets::NoBlend();

		PsoOptions opt(
			D3D12_CULL_MODE_NONE,
			D3D12_DEPTH_WRITE_MASK_ZERO,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			false
		);

		auto rtv = RTVFormatOptions::AllSame(5, DXGI_FORMAT_R8G8B8A8_UNORM);

		auto vsPera1 = Shader::CompileFromFile(L"peraVertex.hlsl", "vs", "vs_5_0");

		//MRT用
		{
			D3D12_STATIC_SAMPLER_DESC samplerMRT = CD3DX12_STATIC_SAMPLER_DESC(0);
			samplerMRT.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerMRT.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;


			//ルートシグネチャ作成
			auto rootSig = CreatePostProcessRootSignature(ctx.device, rootParam, &samplerMRT, 1);
			out.SetRootSig(RootSigId::PostProcess_MRT, rootSig);

			//バーテックスシェーダとピクセルシェーダコンパイル
			auto psPera1 = Shader::CompileFromFile(L"peraPixel.hlsl", "ps", "ps_5_0");
			auto pera1Pso = CreatePostProcessPso(ctx, rootSig.Get(), vsPera1.Get(), psPera1.Get(), blendDesc, opt, rtv);
			out.SetPso(PsoId::PostProcess_Pera1, pera1Pso);
			//最終合成用
			auto psPera2 = Shader::CompileFromFile(L"peraPixel.hlsl", "psFinalDraw", "ps_5_0");
			auto pera2Pso = CreatePostProcessPso(ctx, rootSig.Get(), vsPera1.Get(), psPera2.Get(), blendDesc, opt, rtv);
			out.SetPso(PsoId::PostProcess_Pera2, pera2Pso);

			//ブレ描画用
			auto psBlur = Shader::CompileFromFile(L"peraPixel.hlsl", "BlurPS", "ps_5_0");
			auto blurPso = CreatePostProcessPso(ctx, rootSig.Get(), vsPera1.Get(), psBlur.Get(), blendDesc, opt, rtv);
			out.SetPso(PsoId::PostProcess_Blur, blurPso);


			//SSAO用
			RTVFormatOptions rtvSSAO{};
			rtvSSAO.numRTV = 1;
			rtvSSAO.formats[0] = DXGI_FORMAT_R32_FLOAT;
			auto psSSAO = Shader::CompileFromFile(L"SSAOPixelShader.hlsl", "SsaoPS", "ps_5_0");
			auto ssaoPso = CreateSsaoGeneratePso(ctx, rootSig.Get(), vsPera1.Get(), psSSAO.Get(), rtvSSAO);
			out.SetPso(PsoId::PostProcess_SSAO, ssaoPso);




		}


		//ディファードレンダリング用
		{
			RTVFormatOptions rtvDeffered{};
			rtvDeffered.AllSame;
			rtvDeffered.numRTV = 2;
			rtvDeffered.formats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			rtvDeffered.formats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;

			//サンプラーの生成
			//テクスチャデータからどう色を取り出すかを決めるための設定
			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.MipLODBias = 0;
			sampler.MaxAnisotropy = 0;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.ShaderRegister = 0;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			//影用のサンプラー
			D3D12_STATIC_SAMPLER_DESC samplerShadow = {};
			samplerShadow.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerShadow.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerShadow.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerShadow.MipLODBias = 0;
			samplerShadow.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerShadow.MinLOD = 0.0f;
			samplerShadow.MaxLOD = D3D12_FLOAT32_MAX;
			samplerShadow.RegisterSpace = 0;
			samplerShadow.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			//<=であればtrue(1.0)そうでなければ(0.0)
			samplerShadow.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			samplerShadow.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;	//比較結果をバイリニア補完
			samplerShadow.MaxAnisotropy = 1; //深度傾斜を有効に
			samplerShadow.ShaderRegister = 1;

			D3D12_STATIC_SAMPLER_DESC samplers[2] = { sampler, samplerShadow };

			//ルートシグネチャ作成
			auto rootSigDeferred = CreatePostProcessRootSignature(ctx.device, rootParam, samplers, _countof(samplers));
			out.SetRootSig(RootSigId::PostProcess_Deferred, rootSigDeferred);

			auto psDeferred = Shader::CompileFromFile(L"DeferredRenderingPixelShader.hlsl", "DeferredRendering", "ps_5_0");
			auto deferredPso = CreatePostProcessPso(ctx, rootSigDeferred.Get(), vsPera1.Get(), psDeferred.Get(), blendDesc, opt, rtvDeffered);
			out.SetPso(PsoId::PostProcess_Deferred, deferredPso);

		}

		return true;
	}

	PostProcessBuilder::PostProcessRootParams PostProcessBuilder::MakePostProcessRootParams()
	{
		PostProcessRootParams p{};

		//rangesを作る
		//CBV
		p.cbv[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, MRT_CBV_NUM, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t0..t5)
		p.srv0[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_MRT, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t6)
		p.srv1[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t7)
		p.srv2[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t8) depth
		p.srv3[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t9) light depth
		p.lightDepth[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t10) ssao
		p.ssao[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t11) deferred
		p.deferred[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t12) primitive
		p.primitive[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 12, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t13) lighting
		p.lighting[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 13, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t14) afterLightingColor
		p.afterLightingColor[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 14, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		//SRV(t15) afterLightingHighLum
		p.afterLightingHighLum[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 15, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);


		//root paramsをrangesに紐付ける（DescriptorTable）
		p.params[0].InitAsDescriptorTable(1, p.cbv.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[1].InitAsDescriptorTable(1, p.srv0.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[2].InitAsDescriptorTable(1, p.srv1.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[3].InitAsDescriptorTable(1, p.srv2.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[4].InitAsDescriptorTable(1, p.srv3.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[5].InitAsDescriptorTable(1, p.lightDepth.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[6].InitAsDescriptorTable(1, p.ssao.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[7].InitAsDescriptorTable(1, p.deferred.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[8].InitAsDescriptorTable(1, p.primitive.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[9].InitAsDescriptorTable(1, p.lighting.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[10].InitAsDescriptorTable(1, p.afterLightingColor.data(), D3D12_SHADER_VISIBILITY_PIXEL);
		p.params[11].InitAsDescriptorTable(1, p.afterLightingHighLum.data(), D3D12_SHADER_VISIBILITY_PIXEL);

		return p;
	}



	ComPtr<ID3D12RootSignature> PostProcessBuilder::CreatePostProcessRootSignature(
		ID3D12Device* device,
		const PostProcessRootParams& rp,
		const D3D12_STATIC_SAMPLER_DESC* samplers,
		UINT numSamplers)
	{



		//ルートパラメータ、サンプラーからルートシグネチャを生成
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(static_cast<UINT>(rp.params.size()), rp.params.data(), numSamplers, samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


		ComPtr<ID3DBlob> blob;
		ComPtr<ID3DBlob> err;

		HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err);

		if (FAILED(hr))
		{
			if (err)
				OutputDebugStringA(reinterpret_cast<const char*>(err->GetBufferPointer()));
			return nullptr;
		}

		ComPtr<ID3D12RootSignature> rootSig;
		ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSig.ReleaseAndGetAddressOf())));


		return  rootSig;
	}

	D3D12_INPUT_LAYOUT_DESC PostProcessBuilder::MakePostProcessInputLayout()
	{
		inputElements_.clear();
		inputElements_.reserve(2);

		//ペラポリゴン用
		inputElements_.push_back({ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 });
		inputElements_.push_back({ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 });

		D3D12_INPUT_LAYOUT_DESC desc{};
		desc.pInputElementDescs = inputElements_.data();
		desc.NumElements = static_cast<UINT>(inputElements_.size());
		return desc;

	}


	ComPtr<ID3D12PipelineState> PostProcessBuilder::CreatePostProcessPso(
		const PipelineBuildContext& ctx,
		ID3D12RootSignature* rootSig,
		ID3DBlob* vs,
		ID3DBlob* ps,
		const D3D12_BLEND_DESC& blendDesc,
		const PsoOptions& option,
		const RTVFormatOptions& rtvFormatOption)
	{

		//ブレンドしないFBX用パイプライン
	//裏画面描画する
		auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		//バーテックスバッファの構成設定
		const auto inputLayout = MakePostProcessInputLayout();


		//パイプラインステートオブジェクト（PSO）を生成
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = inputLayout;	//入力レイアウトの設定
		psoDesc.pRootSignature = rootSig;
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vs);	//頂点シェーダ
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(ps);	//ピクセルシェーダ
		psoDesc.RasterizerState = rasterizerState;	//ラスタライザーステート
		psoDesc.RasterizerState.CullMode = rasterizerState.CullMode;
		psoDesc.RasterizerState.FrontCounterClockwise = false;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);	//ブレンドステート
		//深度ステンシル
		psoDesc.DepthStencilState.DepthEnable = false;		//深度バッファーを使用するか
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 小さい方を採用する
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;	//書き込む
		psoDesc.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		psoDesc.DepthStencilState.BackFace = { D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		psoDesc.DepthStencilState.StencilEnable = false;	//ステンシルテストを行うか
		psoDesc.SampleMask = UINT_MAX;	//サンプルマスクの設定

		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;	//トポロジタイプ
		psoDesc.NumRenderTargets = rtvFormatOption.numRTV;	//レンダーターゲット数
		for (int i = 0; i < rtvFormatOption.numRTV; i++) {
			psoDesc.RTVFormats[i] = rtvFormatOption.formats[i];
		}

		psoDesc.SampleDesc.Count = 1; //マルチサンプリングの設定
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;	//深度バッファーで使用するフォーマット



		ComPtr<ID3D12PipelineState> pso;

		ThrowIfFailed(ctx.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));

		return pso;
	}

	ComPtr<ID3D12PipelineState> PostProcessBuilder::CreateSsaoGeneratePso(
		const PipelineBuildContext& ctx,
		ID3D12RootSignature* rootSig,
		ID3DBlob* vs,
		ID3DBlob* ps,
		const RTVFormatOptions& rtvFormatOption)
	{
		auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerState.FrontCounterClockwise = FALSE;

		//ポストプロセス用の入力レイアウト
		const auto inputLayout = MakePostProcessInputLayout();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = inputLayout;
		psoDesc.pRootSignature = rootSig;
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vs);
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(ps);

		psoDesc.RasterizerState = rasterizerState;

		//Blend：SSAO生成は通常ブレンド無し
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

		//Depth/Stencil：SSAO生成は通常不要
		//未初期化事故を避けるため、いったんDEFAULTで埋めてから無効化
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		psoDesc.DepthStencilState.StencilEnable = FALSE;

		psoDesc.SampleMask = UINT_MAX;

		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		//RTVフォーマット：AO出力 
		psoDesc.NumRenderTargets = rtvFormatOption.numRTV;
		for (int i = 0; i < rtvFormatOption.numRTV; ++i)
		{
			psoDesc.RTVFormats[i] = rtvFormatOption.formats[i];
		}

		psoDesc.SampleDesc.Count = 1;

		//Depthを使わないからUNKNOWNを使う
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

		ComPtr<ID3D12PipelineState> pso;
		ThrowIfFailed(ctx.device->CreateGraphicsPipelineState(
			&psoDesc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));

		return pso;
	}

}