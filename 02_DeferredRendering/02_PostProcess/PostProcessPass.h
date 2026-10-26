#pragma once
#include "PostProcessContext.h"

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <cstdint>
#include <DirectXMath.h>
#include<array>
#include<string>
#include"IPostProcess.h"
#include "ConstantBuffer.h" 

struct SsaoInfo;


namespace Engine::Render::PostProcess
{


    class PostProcess : public IPostProcess
    {
    public:
        PostProcess() = default;
        ~PostProcess() = default;

        //生成（ヒープ/中間RT/CB/VBなど）
        void Init(const PostProcessContext& ctx) override;

        //リサイズ（RT作り直し）
        void OnResize(const PostProcessContext& ctx) override;

        //GBuffer 書き込み開始/終了（ここで MRT を RT にセットする）
        void BeginGBuffer(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx) override;
        void EndGBuffer(ID3D12GraphicsCommandList* cmd) override;



        //ポストプロセス一括（AO -> Deferred -> Bloom/DOF -> Final など）
        void Execute(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx) override;

        //ポストプロセス最終合成これは、レンダーターゲットがメイン描画になるため独立させる
        void DrawFinalComposite(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx) override;

        void UpdateConstantBuffer(PostProcessCBId id, const void* data, size_t bytes) override;

        //レンダーターゲットであるプリミティブ描画に使用するリソース取得関数
        ID3D12Resource* GetPrimitiveTarget()const override;


        D3D12_CPU_DESCRIPTOR_HANDLE GetPrimitiveRTV(RtvSlot rtvSlot)const override;

        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& GetPeraSRVHeap() const 
        {
			return peraSRVHeap_;
        }

    private:

        //Init内で呼ばれる生成系
        void CreateHeaps(const PostProcessContext& ctx);
        void CreatePeraVertex(const PostProcessContext& ctx);

        void CreatePeraResourcesAndView(const PostProcessContext& ctx);
        void CreateDepthSRV(const PostProcessContext& ctx,const Microsoft::WRL::ComPtr<ID3D12Resource>& res, D3D12_CPU_DESCRIPTOR_HANDLE& handle);
        void CreateAmbientOcclusionBuffer(const PostProcessContext& ctx);
        void CreateAmbientOcclusionView(const PostProcessContext& ctx, D3D12_CPU_DESCRIPTOR_HANDLE& handleSRV);
        void CreateEffectBufferAndView(const PostProcessContext& ctx, std::wstring texName, ComPtr<ID3D12Resource>& res, D3D12_CPU_DESCRIPTOR_HANDLE& handle); // bloom/dof等
        void CreateBlurForDOFBuffer(const PostProcessContext& ctx);
        void CreateBokeParamResource(const PostProcessContext& ctx);

        //描画系
        void DrawAmbientOcclusion(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx);
        void DrawShrinkTextureForBlur(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx);
        void DrawHorizonBokeh(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx);
        // GBuffer 書き込み開始/終了（ここで MRT を RT にセットする）ライティング後用
        void BeginAfterLightingGBuffer(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx);
        void EndAfterLightingGBuffer(ID3D12GraphicsCommandList* cmd);

        //ディファードレンダリングはライティング後のGbuffer作成処理で挟む
        void DrawDeferredRendering(ID3D12GraphicsCommandList* cmd, const PostProcessContext& ctx);


        //ヘルパー
        //D3D12_CPU_DESCRIPTOR_HANDLE RtvCPU(RtvIndex idx) const;
        //D3D12_GPU_DESCRIPTOR_HANDLE SrvGPU(SrvIndex idx) const;
        //D3D12_GPU_DESCRIPTOR_HANDLE CbvGPU(CbvIndex idx) const;

        uint32_t RtvInc() const { return rtvIncSize_; }
        uint32_t SrvInc() const { return srvIncSize_; }

    private:
        DeferredRendering deferredRendering;

    private:
        //外部依存（所有しない）
        ID3D12Device* device_ = nullptr;
        CreateResource* createResource_ = nullptr;
        LoadTex* loadTex_ = nullptr;

        Engine::Render::Pipeline::PipelineManager* pipelineManager_ = nullptr;

        //ディスクリプターヒープ
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> peraSRVHeap_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> peraRTVHeap_;
        //アンビエントオクルージョンで使用するヒープ
        //RTVのDH
        ComPtr<ID3D12DescriptorHeap> aoRTVDH_;

        uint32_t rtvIncSize_ = 0;
        uint32_t srvIncSize_ = 0;

        float clsClr_[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float primitiveClear_[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; //プリミティブ描画用のクリアカラー
        float albedoClear_[4] = { 0.55f, 0.75f, 1.0f, 1.0f }; //空色
        float normalClear_[4] = { 0.5f, 0.5f, 1.0f, 1.0f };   //法線っぽい値
        float specClear_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float bloomClear_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };


        //中間リソース
        Microsoft::WRL::ComPtr<ID3D12Resource> gbufAlbedo_;
        Microsoft::WRL::ComPtr<ID3D12Resource> gbufNormal_;
        Microsoft::WRL::ComPtr<ID3D12Resource> gbufSpec_;
        Microsoft::WRL::ComPtr<ID3D12Resource> gbufEmissive_;

        Microsoft::WRL::ComPtr<ID3D12Resource> deferredOut_;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> bloomBuffers_;
        Microsoft::WRL::ComPtr<ID3D12Resource> dof_;
        Microsoft::WRL::ComPtr<ID3D12Resource> ao_;
        Microsoft::WRL::ComPtr<ID3D12Resource> pera2_;

        //Primitive側に描かせる用途
        Microsoft::WRL::ComPtr<ID3D12Resource> primitiveTexRes_;

        //ライティング後の色用のRT
        Microsoft::WRL::ComPtr<ID3D12Resource> lightingResultColorRT;
        //ライティング後の輝度が高い部分のRT
        Microsoft::WRL::ComPtr<ID3D12Resource> lightingResulthighLumRT;


        //ポストエフェクト用のテクスチャリソース
        ComPtr<ID3D12Resource> effectTexBuffer_;

		//ライティング用のストラクチャーバッファ
        ComPtr<ID3D12Resource> lightingBuffer_;


        //定数バッファ等
        Engine::Render::ConstantBuffer<float>        bokehCB_;
        Engine::Render::ConstantBuffer<SsaoInfo>          ssaoCB_;
        Engine::Render::ConstantBuffer<DeferredRendering> deferredCB_;
        Engine::Render::ConstantBuffer<PostProcessDebugCB> postProcessDebugCB_;

        //Fullscreen quad
        Microsoft::WRL::ComPtr<ID3D12Resource> peraVB_;
        D3D12_VERTEX_BUFFER_VIEW peraVBV_{};

        //サイズ
        uint32_t width_ = 0;
        uint32_t height_ = 0;
    };

}