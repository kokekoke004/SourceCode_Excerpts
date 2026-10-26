#include "PostProcessPass.h"

#include "CreateResource.h"
#include "loadTexture.h"
#include "pipelineStateObject.h"
#include"GraphicsUtility.h"
#include"PipelineManager.h"
#include <algorithm>
#include <iterator>

using Microsoft::WRL::ComPtr;
using namespace std;

using namespace Engine::Render::Pipeline;




namespace Engine::Render::PostProcess 
{

    void PostProcess::Init(const PostProcessContext& ctx)
    {
        device_ = ctx.device;
        createResource_ = ctx.createResource;
        loadTex_ = ctx.loadTex;
        pipelineManager_ = ctx.pipelineManager;

        rtvIncSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        srvIncSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        CreateHeaps(ctx);
        CreatePeraVertex(ctx);

        CreateBokeParamResource(ctx);

        CreatePeraResourcesAndView(ctx);
    }

    ID3D12Resource* PostProcess::GetPrimitiveTarget()const
    {
        return primitiveTexRes_.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE PostProcess::GetPrimitiveRTV(RtvSlot rtvSlot)const
    {
        auto h = peraRTVHeap_->GetCPUDescriptorHandleForHeapStart();
        h.ptr += rtvIncSize_ * static_cast<uint32_t>(rtvSlot);
        return h;
    }


    void PostProcess::UpdateConstantBuffer(PostProcessCBId id, const void* data, size_t bytes)
    {
        if (!data) return;

        switch (id)
        {
        case PostProcessCBId::Bokeh:
            if (bytes == sizeof(float)) bokehCB_.Update(*reinterpret_cast<const float*>(data));
            break;

        case PostProcessCBId::Ssao:
            if (bytes == sizeof(SsaoInfo)) ssaoCB_.Update(*reinterpret_cast<const SsaoInfo*>(data));
            break;

        case PostProcessCBId::Deferred:
            if (bytes == sizeof(DeferredRendering)) deferredCB_.Update(*reinterpret_cast<const DeferredRendering*>(data));
            break;
		case PostProcessCBId::PostProcessDebug:
            if (bytes == sizeof(PostProcessDebugCB)) postProcessDebugCB_.Update(*reinterpret_cast<const PostProcessDebugCB*>(data));
            break;
        }
    }


    void PostProcess::OnResize(const PostProcessContext& ctx)
    {

        CreateBokeParamResource(ctx);
        CreatePeraResourcesAndView(ctx);
    }

    void PostProcess::BeginGBuffer(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        //Gバッファー
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(gbufAlbedo_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(gbufNormal_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(gbufSpec_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(bloomBuffers_[0].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &barrier);


        //通常描画用深度
        auto dsvHeapPointer = ctx.dsvHeap_->GetCPUDescriptorHandleForHeapStart();

        //rtvハンドルの要素確保
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvsHandle[4];

        //rtvヒープのスタートアドレス取得
        D3D12_CPU_DESCRIPTOR_HANDLE baseH = peraRTVHeap_->GetCPUDescriptorHandleForHeapStart();

        auto incSize = ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        UINT offset = 0;
        for (auto& handle : rtvsHandle) {
            handle.InitOffsetted(baseH, offset);
            offset += incSize;
        }

        //FBXのRTV設定
        cmd->OMSetRenderTargets(_countof(rtvsHandle), rtvsHandle, false, &dsvHeapPointer);

        //クリアカラー
        cmd->ClearRenderTargetView(rtvsHandle[0], albedoClear_, 0, nullptr);
        cmd->ClearRenderTargetView(rtvsHandle[1], normalClear_, 0, nullptr);
        cmd->ClearRenderTargetView(rtvsHandle[2], specClear_, 0, nullptr);
        cmd->ClearRenderTargetView(rtvsHandle[3], bloomClear_, 0, nullptr);

        cmd->ClearDepthStencilView(dsvHeapPointer,
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    void PostProcess::EndGBuffer(ID3D12GraphicsCommandList* cmd)
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(gbufAlbedo_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(gbufNormal_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(gbufSpec_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);

    }

    void PostProcess::BeginAfterLightingGBuffer(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        //Gバッファー
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(lightingResultColorRT.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(lightingResulthighLumRT.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &barrier);

        //rtvハンドルの要素確保
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvsHandle[2];

        //rtvヒープのスタートアドレス取得
        D3D12_CPU_DESCRIPTOR_HANDLE baseH = peraRTVHeap_->GetCPUDescriptorHandleForHeapStart();

        auto incSize = ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        //ライティング後の位置にアドレス移動する
        baseH.ptr += incSize * MRT_AfterLIghting;

        UINT offset = 0 ;
        for (auto& handle : rtvsHandle) {
            handle.InitOffsetted(baseH, offset);
            offset += incSize;
        }

        //FBXのRTV設定
        cmd->OMSetRenderTargets(_countof(rtvsHandle), rtvsHandle, false, nullptr);

        //クリアカラー
        cmd->ClearRenderTargetView(rtvsHandle[0], clsClr_, 0, nullptr);
        cmd->ClearRenderTargetView(rtvsHandle[1], clsClr_, 0, nullptr);

    }

    void PostProcess::EndAfterLightingGBuffer(ID3D12GraphicsCommandList* cmd)
    {
        //Gバッファー
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(lightingResultColorRT.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(lightingResulthighLumRT.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);

    }

    void PostProcess::Execute(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        // ここで srvHeap をセット（PostProcess で統一）
        ID3D12DescriptorHeap* heaps[] = { peraSRVHeap_.Get() };
        cmd->SetDescriptorHeaps(_countof(heaps), heaps);

        DrawHorizonBokeh(cmd, ctx);
        DrawAmbientOcclusion(cmd, ctx);
        DrawDeferredRendering(cmd, ctx);
        DrawShrinkTextureForBlur(cmd, ctx);
    }


    void PostProcess::CreateHeaps(const PostProcessContext& ctx)
    {
        // RTV Heap（Count個）
        //{
        //    D3D12_DESCRIPTOR_HEAP_DESC desc{};
        //    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        //    desc.NumDescriptors = static_cast<uint32_t>(RtvIndex::Count);
        //    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        //    device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvHeap_.ReleaseAndGetAddressOf()));
        //}

        // SRV Heap（SRV + CBV を同じ heap へ）
        //{
        //    D3D12_DESCRIPTOR_HEAP_DESC desc{};
        //    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        //    desc.NumDescriptors =
        //        static_cast<uint32_t>(SrvIndex::Count) +
        //        static_cast<uint32_t>(CbvIndex::Count);
        //    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        //    device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(srvHeap_.ReleaseAndGetAddressOf()));
        //}

    }

    void PostProcess::CreatePeraVertex(const PostProcessContext& ctx)
    {

        struct PeraVertex
        {
            DirectX::XMFLOAT3 pos;
            DirectX::XMFLOAT2 uv;
        };

        PeraVertex pv[4] = { {{-1,-1,0.1},{0,1}},	//左下
                            {{-1,1,0.1},{0,0}},		//左上
                            {{1,-1,0.1},{1,1}},		//右下
                            {{1,1,0.1},{1,0}} };	//左下

        //ペラポリゴン用のリソース生成
        peraVB_ = ctx.createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeof(pv), D3D12_RESOURCE_STATE_GENERIC_READ);

        //map処理
        PeraVertex* mappedPera = nullptr;
        peraVB_->Map(0, nullptr, (void**)&mappedPera);
        copy(begin(pv), end(pv), mappedPera);
        peraVB_->Unmap(0, nullptr);

        peraVBV_.BufferLocation = peraVB_->GetGPUVirtualAddress();
        peraVBV_.SizeInBytes = sizeof(pv);
        peraVBV_.StrideInBytes = sizeof(PeraVertex);
    }

    void PostProcess::CreatePeraResourcesAndView(const PostProcessContext& ctx)
    {

        auto& bbuff = ctx.renderTargets[0];
        auto resDesc = bbuff->GetDesc();	//もともと使用しているバッファの情報取得



        //マルチレンダーターゲットで使用するリソース生成
        //レンダーターゲット構成
        //rtv1 : 通常描画
        //rtv2 : 法線
        //rtv3 : スペキュラ
        //rtv4 : 高輝度
        //rtv5 : 縮小バッファ　高輝度
        //rtv6 : 縮小バッファ


        //１～3

        //複数のpera1Respurcesのリソース作成
        gbufAlbedo_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, albedoClear_, DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);


        gbufNormal_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, normalClear_, DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);

        gbufSpec_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, specClear_, DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);


        //ブルーム用バッファの作成
        //4～5
        for (auto& res : bloomBuffers_) {
            res = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, bloomClear_, DXGI_FORMAT_R8G8B8A8_UNORM,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);
        }

        //6つ目(被写界深度用縮小バッファRT)
        CreateBlurForDOFBuffer(ctx);

        //2枚目
        pera2_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, clsClr_, DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);


        //アンビエントオクルージョンバッファー作成
        CreateAmbientOcclusionBuffer(ctx);

        //ディファードレンダリングリソース作成
        deferredOut_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, clsClr_, DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);

        primitiveTexRes_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, primitiveClear_, DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);

		//ライティング用のインスタンスバッファー・ビュー作成はライトマネージャクラスでライト設置時に行うため、ここでは行わない

        auto resDescAfterLighting = resDesc;
        resDescAfterLighting.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

        //ライティング後の色
        lightingResultColorRT = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, clsClr_, DXGI_FORMAT_R16G16B16A16_FLOAT,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDescAfterLighting);
        //ライティング後の高輝度
        lightingResulthighLumRT = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, clsClr_, DXGI_FORMAT_R16G16B16A16_FLOAT,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDescAfterLighting);


        //createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT,clsClr,
            //D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,)

        //レンダーターゲットビュー（RTV）を作る
        //その前に、スクリプタヒープが必要
        //もともと作っていたヒープの情報でもう一枚作る
        auto heapDesc = ctx.rtvHeaps_->GetDesc();
        heapDesc.NumDescriptors = 11;	//MRTで用いるRTV4つとブルームで用いるRTV2つ、被写界深度縮小バッファ1つ
        ThrowIfFailed(ctx.device->CreateDescriptorHeap(&heapDesc,
            IID_PPV_ARGS(peraRTVHeap_.ReleaseAndGetAddressOf())), L"\n_peraRTVHeap作成に失敗しました");

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        auto handle = peraRTVHeap_->GetCPUDescriptorHandleForHeapStart();

        //RTV作成////////////////////////////////////////////////////
        //まだデスクリプタヒープのみで、ビューを作っていないので作る
        //ペラポリゴン1枚目(1から3)
        //2つの_pera1ResourcesのRTVビューを作成
        //1,2,3枚目
        ctx.createResource->CreateRTV_View(
            gbufAlbedo_.Get(),
            rtvDesc,
            handle
        );

        ctx.createResource->CreateRTV_View(
            gbufNormal_.Get(),
            rtvDesc,
            handle
        );

        ctx.createResource->CreateRTV_View(
            gbufSpec_.Get(),
            rtvDesc,
            handle
        );


        //4枚目：ブルーム用１
        ctx.createResource->CreateRTV_View(
            bloomBuffers_[0].Get(),
            rtvDesc,
            handle
        );

        //5枚目ブルーム用縮小バッファ
        ctx.createResource->CreateRTV_View(
            bloomBuffers_[1].Get(),
            rtvDesc,
            handle
        );

        //6枚目被写界深度縮小テクスチャ
        ctx.createResource->CreateRTV_View(
            dof_,
            rtvDesc,
            handle
        );

        //7
        ctx.createResource->CreateRTV_View(
            pera2_.Get(),
            rtvDesc,
            handle
        );

        //8
        ctx.createResource->CreateRTV_View(
            deferredOut_.Get(),
            rtvDesc,
            handle
        );

        //9
        ctx.createResource->CreateRTV_View(
            primitiveTexRes_.Get(),
            rtvDesc,
            handle
        );

        auto rtvAfetrLighting = rtvDesc;
        rtvAfetrLighting.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        //10: ライティング後の色
        ctx.createResource->CreateRTV_View(
            lightingResultColorRT.Get(),
            rtvAfetrLighting,
            handle
        );

        //11：ライティング後の高輝度
        ctx.createResource->CreateRTV_View(
            lightingResulthighLumRT.Get(),
            rtvAfetrLighting,
            handle
        );


        ///////////////////////////////////////////
        //シェーダーSSAOにデータ送る用CB
        ssaoCB_.Create(ctx.device);
        //ssaoConstBuffer_ = ctx.createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, alignmentSize(sizeof(SsaoInfo)), D3D12_RESOURCE_STATE_GENERIC_READ);
        //シェーダーコンスタントバッファに送る用のCB
        deferredCB_.Create(ctx.device);
        //ボケ用の定数リソースビュー
        bokehCB_.Create(ctx.device);
		//ポストプロセス切り替えのための定数バッファ
        postProcessDebugCB_.Create(ctx.device);

        //deferredRendBuffer_ = ctx.createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, alignmentSize(sizeof(DeferredRendering)), D3D12_RESOURCE_STATE_GENERIC_READ);

        ////////////////////////////////////////////
        //SRV用とCBVヒープを作成する
        //SRV : 16 , CBV : 4
        //SRVの５と６で通常描画とライトのデプス管理
        //CBV1 : ガウシアンぼかしを受け取るための定数バッファ
        //CBV2 : SSAOの逆行列などを渡す
        //CBV3 : ディファードレンダリングで使用する定数バッファ
		//CBV4 : ポストプロセス切り替え用定数バッファ
        //SRV1 : ペラポリゴン1用
        //SRV2 : ペラポリゴン1用
        //SRV3 : ペラポリゴン１用　スペキュラ
        //SRV4 : ブルーム用 1 :輝度テクスチャとして使う
        //SRV5 : ブルーム用 2 :高輝度縮小テクスチャ
        //SRV6 : 被写界深度縮小テクスチャ
        //SRV7 : ペラポリゴン1用(横ボケで使う)
        //SRV8 : ペラポリゴン1で用いるノーマルマップを用いたポストエフェクト用
        //SRV9 : 通常深度値
        //SRV10 : ライト目線の深度値 
        //SRV11 : アンビエントオクルージョン
        //SRV12 : ディファードレンダリング
        //SRV13 : プリミティブ描画から作成するテクスチャ
		//SRV14 : ライティング用のストラクチャーバッファ
        //SRV15 : ライティング後のカラー
        //SRV16 : ライティング後の高輝度

        heapDesc.NumDescriptors = 20;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        ThrowIfFailed(ctx.device->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(peraSRVHeap_.ReleaseAndGetAddressOf())
        ), L"\n_peraSRVHeapの作成に失敗しました");

        //SRVビューを作成する
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = rtvDesc.Format;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto srvHeapPointer = peraSRVHeap_->GetCPUDescriptorHandleForHeapStart();

        //CBVリソースビューを作成する/////////////////////
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = bokehCB_.Get()->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = bokehCB_.Get()->GetDesc().Width;

        ctx.createResource->CreateCustomConstantBufferView(bokehCB_.Get(), cbvDesc, srvHeapPointer); //b0

        cbvDesc.BufferLocation = ssaoCB_.Get()->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = ssaoCB_.Get()->GetDesc().Width;

        ctx.createResource->CreateCustomConstantBufferView(ssaoCB_.Get(), cbvDesc, srvHeapPointer); //b1

        cbvDesc.BufferLocation = deferredCB_.Get()->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = deferredCB_.Get()->GetDesc().Width;

        ctx.createResource->CreateCustomConstantBufferView(deferredCB_.Get(), cbvDesc, srvHeapPointer); //b2

        cbvDesc.BufferLocation = postProcessDebugCB_.Get()->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = postProcessDebugCB_.Get()->GetDesc().Width;
        ctx.createResource->CreateCustomConstantBufferView(postProcessDebugCB_.Get(), cbvDesc, srvHeapPointer); //b3

        ///////////////////////////////////////////////

        //SRVリソースビューを作成する////////////////////
        //1,2,3枚目 MRTで用いる２つのリソースビューを作成する t0,t1,t2
        ctx.createResource->CreateCustomShaderResourceView(gbufAlbedo_, srvDesc, srvHeapPointer);
        ctx.createResource->CreateCustomShaderResourceView(gbufNormal_, srvDesc, srvHeapPointer);
        ctx.createResource->CreateCustomShaderResourceView(gbufSpec_, srvDesc, srvHeapPointer);


        //4:ブルーム用のリソースビュー作成 t3
        ctx.createResource->CreateCustomShaderResourceView(bloomBuffers_[0], srvDesc, srvHeapPointer);

        //5:ブルーム用高輝度縮小テクスチャ t4
        ctx.createResource->CreateCustomShaderResourceView(bloomBuffers_[1], srvDesc, srvHeapPointer);

        //6:被写界深度縮小縮小テクスチャ t5
        ctx.createResource->CreateCustomShaderResourceView(dof_, srvDesc, srvHeapPointer);

        //7 t6
        ctx.createResource->CreateCustomShaderResourceView(pera2_, srvDesc, srvHeapPointer);

        //8 : ポストエフェクトノーマルマップのリソースとリソースビュー作成 t7
        CreateEffectBufferAndView(ctx,L"Assets/normal/normalmap.jpg", effectTexBuffer_, srvHeapPointer);

        //シェーダーに深度値を渡すリソースの生成
        //9 : 通常 t8
        CreateDepthSRV(ctx,ctx.depth,srvHeapPointer);
        //10 : ライトデプス t9
        CreateDepthSRV(ctx, ctx.lightDepth, srvHeapPointer);
        //11 : SSAO t10
        CreateAmbientOcclusionView(ctx,srvHeapPointer);
        //12 : ディファードレンダリング t11
        ctx.createResource->CreateCustomShaderResourceView(deferredOut_, srvDesc, srvHeapPointer);
        //13 : プリミティブテクスチャ t12
        ctx.createResource->CreateCustomShaderResourceView(primitiveTexRes_, srvDesc, srvHeapPointer);

		//14 : ライティング用のストラクチャーバッファ t13
		//ライティング用のストラクチャーバッファはライトマネージャクラスで作成するため、ここでは作らない
        srvHeapPointer.ptr += ctx.createResource->GetIncSize();

        auto srvAfetrLighting = srvDesc;
        srvAfetrLighting.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        //15 : ライティング後のカラー t14
        ctx.createResource->CreateCustomShaderResourceView(lightingResultColorRT, srvAfetrLighting, srvHeapPointer);

        //16 : ライティング後のカラー t15
        ctx.createResource->CreateCustomShaderResourceView(lightingResulthighLumRT, srvAfetrLighting, srvHeapPointer);


        //////////////////////////////////////////////

    }

    void PostProcess::CreateDepthSRV(const PostProcessContext& ctx,const Microsoft::WRL::ComPtr<ID3D12Resource>& res,D3D12_CPU_DESCRIPTOR_HANDLE& handle)
    {
        //depth(SRV) 作成：SrvIndex::Depth に固定
        D3D12_SHADER_RESOURCE_VIEW_DESC resDesc = {};
        resDesc.Format = DXGI_FORMAT_R32_FLOAT;
        resDesc.Texture2D.MipLevels = 1;
        resDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        resDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        ctx.createResource->CreateCustomShaderResourceView(res, resDesc, handle);
    }

    void PostProcess::CreateAmbientOcclusionBuffer(const PostProcessContext& ctx)
    {
        //ao_ リソース作成
        auto& bbuff = ctx.renderTargets[0];
        auto resDesc = bbuff->GetDesc();	//もともと使用しているバッファの情報取得

        resDesc.Format = DXGI_FORMAT_R32_FLOAT;	//乗算用の明度情報を処理するためR32

        D3D12_HEAP_PROPERTIES heapProp =
            CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        ao_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, clsClr_, resDesc.Format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);

    }

    void PostProcess::CreateAmbientOcclusionView(const PostProcessContext& ctx, D3D12_CPU_DESCRIPTOR_HANDLE& handleSRV)
    {
        //ao_ の RTV を RtvIndex::AO に
        //ao_ の SRV を SrvIndex::AO に

        //RTVヒープの作成//////////////////////////////
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 0;
        desc.NumDescriptors = 1;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ThrowIfFailed(ctx.device->CreateDescriptorHeap(
            &desc,
            IID_PPV_ARGS(aoRTVDH_.ReleaseAndGetAddressOf())
        ));

        //RTV作成///////////////////////////////////////
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        auto handleRTV = aoRTVDH_->GetCPUDescriptorHandleForHeapStart();

        ctx.createResource->CreateRTV_View(ao_, rtvDesc, handleRTV);
        //SRV作成//////////////////////////////////////
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;


        ctx.createResource->CreateCustomShaderResourceView(ao_, srvDesc, handleSRV);

    }

    void PostProcess::CreateEffectBufferAndView(const PostProcessContext& ctx, std::wstring texName, ComPtr<ID3D12Resource>& res, D3D12_CPU_DESCRIPTOR_HANDLE& handle)
    {
        //法線マップをロードする
        res = ctx.loadTex->loadTex(texName);

        //ポストエフェクト用のテクスチャビューを作成する
        ctx.createResource->CreateShaderResourceView(res, handle);
    }

    void PostProcess::CreateBlurForDOFBuffer(const PostProcessContext& ctx)
    {
        //DOF の縮小/ブラー用なら、その中間を作る
        float color[4] = { 0.0f,0.0f,0.0f,1.0f };
        auto& bbuf = ctx.renderTargets[0];
        auto resDesc = bbuf->GetDesc();
        dof_ = ctx.createResource->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, color, resDesc.Format,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, resDesc);


    }

    void PostProcess::CreateBokeParamResource(const PostProcessContext& ctx)
    {
        //bokehParamResource_ 作成 + CBV を CbvIndex::Bokeh に
        auto weights = GetGaussianWeights(5.0f, 8);
        //ボケ情報のリソースを作成する
        bokehCB_.Create(ctx.device);
        float* mappedWeight = nullptr;

        UpdateConstantBuffer(PostProcessCBId::Bokeh, weights.data(), weights.size());
        ////シェーダーへボケのパラメーターを送る
        //ThrowIfFailed(bokehParamResource_->Map(0, nullptr, (void**)&mappedWeight)
        //    , L"\nbokehParamResourceのmapに失敗しました");

        //copy(weights.begin(), weights.end(), mappedWeight);

        //bokehParamResource_->Unmap(0, nullptr);

    }

    void PostProcess::DrawAmbientOcclusion(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        //アンビエントオクルージョンバッファをシェーダーリソースに
        Barrier(cmd, ao_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        //通常描画用深度
        auto dsvHeapPointer = ctx.dsvHeap_->GetCPUDescriptorHandleForHeapStart();


        auto rtvBaceHandle = aoRTVDH_->GetCPUDescriptorHandleForHeapStart();
        auto inc = ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        cmd->OMSetRenderTargets(1, &rtvBaceHandle, false, nullptr);

        //cmd->SetGraphicsRootSignature(rootsignatureMRT_[0].Get());
        cmd->SetGraphicsRootSignature(ctx.pipelineManager->GetRootSig(RootSigId::PostProcess_MRT));

        cmd->RSSetViewports(1, &ctx.viewport);
        cmd->RSSetScissorRects(1, &ctx.scissor);	//シザー矩形

        cmd->SetDescriptorHeaps(1, peraSRVHeap_.GetAddressOf());
        auto srv_cbvBaceHandle = peraSRVHeap_->GetGPUDescriptorHandleForHeapStart();
        cmd->SetGraphicsRootDescriptorTable(0, srv_cbvBaceHandle);
        srv_cbvBaceHandle.ptr += inc * MRT_CBV_NUM;
        //ペラポリゴン側のテーブル 
        //SRV
        //パラメーター０番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(1, srv_cbvBaceHandle);

        srv_cbvBaceHandle.ptr += inc * NUM_MRT;
        cmd->SetGraphicsRootDescriptorTable(2, srv_cbvBaceHandle);

        srv_cbvBaceHandle.ptr += inc;
        cmd->SetGraphicsRootDescriptorTable(3, srv_cbvBaceHandle);

        srv_cbvBaceHandle.ptr += inc;

        //パラメーター3番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(4, srv_cbvBaceHandle);

        srv_cbvBaceHandle.ptr += inc;
        cmd->SetGraphicsRootDescriptorTable(5, srv_cbvBaceHandle);


        //cmd->SetPipelineState(_aoPipeline.Get());
        cmd->SetPipelineState(ctx.pipelineManager->GetPso(PsoId::PostProcess_SSAO));
        
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        cmd->IASetVertexBuffers(0, 1, &peraVBV_);
        cmd->DrawInstanced(4, 1, 0, 0);


        //アンビエントオクルージョンバッファをバッファをシェーダーリソースに
        Barrier(cmd, ao_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void PostProcess::DrawDeferredRendering(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        BeginAfterLightingGBuffer(cmd, ctx);

        cmd->RSSetViewports(1, &ctx.viewport);
        cmd->RSSetScissorRects(1, &ctx.scissor);	//シザー矩形

        //cmd->SetGraphicsRootSignature(rootsignatureDeferred.Get());	//SetGraphicsRootDescriptorTableこの前で呼び出す
        cmd->SetGraphicsRootSignature(ctx.pipelineManager->GetRootSig(RootSigId::PostProcess_Deferred));	//SetGraphicsRootDescriptorTableこの前で呼び出す



        //ヒープをセット
        cmd->SetDescriptorHeaps(1, peraSRVHeap_.GetAddressOf());
        auto handle = peraSRVHeap_->GetGPUDescriptorHandleForHeapStart();
        //CBV
        cmd->SetGraphicsRootDescriptorTable(0, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * MRT_CBV_NUM;
        //SRV
        //パラメーター０番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(1, handle);

        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * NUM_MRT;
        //パラメーター1番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(2, handle);

        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //パラメーター2番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(3, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //パラメーター3番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(4, handle);

        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //パラメーター4番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(5, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //アンビエントオクルージョン
        cmd->SetGraphicsRootDescriptorTable(6, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //t11
        cmd->SetGraphicsRootDescriptorTable(7, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

        //t12
        cmd->SetGraphicsRootDescriptorTable(8, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

        //t13 ライティングをここでセット
        cmd->SetGraphicsRootDescriptorTable(9, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

        //t14 ライティングごのカラーをここでセット
        cmd->SetGraphicsRootDescriptorTable(10, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

        //t15 ライティング後の高輝度をここでセット
        cmd->SetGraphicsRootDescriptorTable(11, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;


        cmd->SetPipelineState(ctx.pipelineManager->GetPso(PsoId::PostProcess_Deferred));

        cmd->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        cmd->IASetVertexBuffers(0, 1, &peraVBV_);

        cmd->DrawInstanced(4, 1, 0, 0);

        EndAfterLightingGBuffer(cmd);

    }

    void PostProcess::DrawShrinkTextureForBlur(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        //CommandListFrom(commandContext)->SetPipelineState(_blurPipeline.Get());
        cmd->SetPipelineState(ctx.pipelineManager->GetPso(PsoId::PostProcess_Blur));
        //CommandListFrom(commandContext)->SetGraphicsRootSignature(rootsignatureMRT_[0].Get());

        cmd->SetGraphicsRootSignature(ctx.pipelineManager->GetRootSig(RootSigId::PostProcess_MRT));

        //頂点バッファーセット
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        cmd->IASetVertexBuffers(0, 1, &peraVBV_);

        //高輝度成分バッファーはシェーダーリソースに
        Barrier(cmd, bloomBuffers_[0].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        //縮小バッファはレンダーターゲットに
        Barrier(cmd, bloomBuffers_[1].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        //被写界深度縮小バッファはレンダーターゲットに
        Barrier(cmd, dof_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);


        auto rtvHandle =
            peraRTVHeap_->GetCPUDescriptorHandleForHeapStart();

        auto srvHandle =
            peraSRVHeap_->GetGPUDescriptorHandleForHeapStart();

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {};


        //RTVを5つ目に移動
        auto rtvInc = ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        rtvHandles[0].InitOffsetted(rtvHandle, rtvInc * 4); //t4
        rtvHandles[1].InitOffsetted(rtvHandle, rtvInc * 5); //t5

        //レンダーターゲットをセット
        cmd->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, false, nullptr);


		for (const auto& rtv : rtvHandles) {
            //レンダーターゲットのクリア
			cmd->ClearRenderTargetView(rtv, bloomClear_, 0, nullptr);
		}



        //テクスチャは、１枚目の2つ目のレンダーターゲットから
        srvHandle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * (MRT_CBV_NUM + MRT_AfterLIghting + 1);

        //1パス目を高輝度テクスチャとして使用
        cmd->SetDescriptorHeaps(1, peraSRVHeap_.GetAddressOf());
        cmd->SetGraphicsRootDescriptorTable(1, srvHandle);

        auto desc = bloomBuffers_[0]->GetDesc();
        D3D12_VIEWPORT vp = {};
        D3D12_RECT sr = {};

        vp.MaxDepth = 1.0f;
        vp.MinDepth = 0.0f;
        vp.Height = desc.Height / 2;
        vp.Width = desc.Width / 2;
        sr.top = 0;
        sr.left = 0;
        sr.right = vp.Width;
        sr.bottom = vp.Height;

        for (int i = 0; i < 8; i++) {
            cmd->RSSetViewports(1, &vp);
            cmd->RSSetScissorRects(1, &sr);
            cmd->DrawInstanced(4, 1, 0, 0);

            //書いたら下にずらして次を書く準備
            sr.top += vp.Height;
            vp.TopLeftX = 0;
            vp.TopLeftY = sr.top;

            //幅も高さも半分に
            vp.Width /= 2;
            vp.Height /= 2;
            sr.bottom = sr.top + vp.Height;

        }

        //縮小バッファをシェーダーリソースに
        Barrier(cmd, bloomBuffers_[1].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        //被写界深度縮小バッファをシェーダーリソースに
        Barrier(cmd, dof_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void PostProcess::DrawHorizonBokeh(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        //2枚目のリソースの状態をレンダーターゲットにする
        Barrier(cmd, pera2_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtvHeapPointer = peraRTVHeap_->GetCPUDescriptorHandleForHeapStart();
        //2枚目のRTVなので進ませてレンダーターゲットを割り当てる
        rtvHeapPointer.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * NUM_MRT;
        //ここで、描画するＲＴＶのセットをする
        cmd->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);


        //クリアカラー
        cmd->ClearRenderTargetView(rtvHeapPointer, clsClr_, 0, nullptr);

        cmd->RSSetViewports(1, &ctx.viewport);
        cmd->RSSetScissorRects(1, &ctx.scissor);	//シザー矩形

        //cmd->SetGraphicsRootSignature(rootsignatureMRT_[0].Get());
        cmd->SetGraphicsRootSignature(ctx.pipelineManager->GetRootSig(RootSigId::PostProcess_MRT));
        
        //ヒープをセット
        cmd->SetDescriptorHeaps(1, peraSRVHeap_.GetAddressOf());
        auto handle = peraSRVHeap_->GetGPUDescriptorHandleForHeapStart();
        //_peraSRVHeapこの二つ目の位置の領域を割り当てる

        //CBV　1つ目
        cmd->SetGraphicsRootDescriptorTable(0, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * MRT_CBV_NUM;
        //SRV　1つ目 (rtvの一つ目を使う)
        cmd->SetGraphicsRootDescriptorTable(1, handle);
        //SRV　２つ目
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        cmd->SetPipelineState(ctx.pipelineManager->GetPso(PsoId::PostProcess_Blur));

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        cmd->IASetVertexBuffers(0, 1, &peraVBV_);
        cmd->DrawInstanced(4, 1, 0, 0);

        Barrier(cmd, pera2_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void PostProcess::DrawFinalComposite(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx)
    {
        //ID3D12DescriptorHeap* ppHeaps[] = { basicHeap_.Get() };
        //commandList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);


        cmd->RSSetViewports(1, &ctx.viewport);
        cmd->RSSetScissorRects(1, &ctx.scissor);	//シザー矩形

        cmd->SetGraphicsRootSignature(ctx.pipelineManager->GetRootSig(RootSigId::PostProcess_MRT));



        //ヒープをセット
        cmd->SetDescriptorHeaps(1, peraSRVHeap_.GetAddressOf());
        auto handle = peraSRVHeap_->GetGPUDescriptorHandleForHeapStart();
        //CBV
        cmd->SetGraphicsRootDescriptorTable(0, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * MRT_CBV_NUM;
        //SRV
        //パラメーター０番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(1, handle);

        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * NUM_MRT;
        //パラメーター1番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(2, handle);

        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //パラメーター2番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(3, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //パラメーター3番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(4, handle);

        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //パラメーター4番とヒープを関連付ける
        cmd->SetGraphicsRootDescriptorTable(5, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //アンビエントオクルージョン
        cmd->SetGraphicsRootDescriptorTable(6, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //ディファードレンダリング
        cmd->SetGraphicsRootDescriptorTable(7, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //プリミティブテクスチャセット
        cmd->SetGraphicsRootDescriptorTable(8, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        //t13 ライティングをここでセット
        cmd->SetGraphicsRootDescriptorTable(9, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

        //t14 ライティングごのカラーをここでセット
        cmd->SetGraphicsRootDescriptorTable(10, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;

        //t15 ライティング後の高輝度をここでセット
        cmd->SetGraphicsRootDescriptorTable(11, handle);
        handle.ptr += ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);;


        cmd->SetPipelineState(ctx.pipelineManager->GetPso(PsoId::PostProcess_Pera2));

        cmd->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        cmd->IASetVertexBuffers(0, 1, &peraVBV_);

        cmd->DrawInstanced(4, 1, 0, 0);
    }

}
