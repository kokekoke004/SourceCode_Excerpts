#include"UiManager.h"
#include"DirectWrite_2D.h"
#include"pipelineStateObject.h"
#include"CreateResource.h"
#include"UI.h"
#include"MouseInput.h"
#include"UIInteraction.h"
#include"TextInputManager.h"
#include"Screenshot.h"

using namespace DirectX;
using namespace std;

UiManager::UiManager(UIRenderContext uiRenderContext)
	:uiRenderContext_(uiRenderContext)
{
	currentScreen_ = nullptr;
	currentKey_ = L"";
	InitUiManager();
}

void UiManager::CreateUiHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = MAX_NUM_UI * UI_ALL_NUM_CBV_SRV;	//ロードするFBXファイルの数だけディスクリプタ作成
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(DeviceFrom(uiRenderContext_.commandContext)->CreateDescriptorHeap(&desc, IID_PPV_ARGS(uiHeap.ReleaseAndGetAddressOf())));
}

void UiManager::CreateUiPipeline()
{
	pipelineStateObject_.reset(new PipelineStateObject(DeviceFrom(uiRenderContext_.commandContext)));

	//ルートシグネチャの生成
	{
		//UI用
		CD3DX12_DESCRIPTOR_RANGE1 discriptorRangesUI[2];			//CBVを一個ずつ使いたいから
		discriptorRangesUI[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UI_NUM_CBV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		discriptorRangesUI[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UI_NUM_SRV, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		CD3DX12_ROOT_PARAMETER1 rootParametersUI[1];	//マルチレンダーターゲットビュー用
		rootParametersUI[0].InitAsDescriptorTable(_countof(discriptorRangesUI), discriptorRangesUI, D3D12_SHADER_VISIBILITY_ALL);
		//////

		//サンプラーの生成
		//テクスチャデータからどう色を取り出すかを決めるための設定
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		//UIルートパラメータ、サンプラーからルートシグネチャを生成
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescUI;
		rootSignatureDescUI.Init_1_1(_countof(rootParametersUI), rootParametersUI, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> rootSigUI_Blob = nullptr;
		ComPtr<ID3DBlob> errorUI_Blob = nullptr;

		checkErrorBloob(D3DX12SerializeVersionedRootSignature(&rootSignatureDescUI, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigUI_Blob, &errorUI_Blob), errorUI_Blob);


		//UI用ルートシグネチャ作成
		ThrowIfFailed(DeviceFrom(uiRenderContext_.commandContext)->CreateRootSignature(0, rootSigUI_Blob->GetBufferPointer(), rootSigUI_Blob->GetBufferSize(), IID_PPV_ARGS(RootSignatureFrom(uiPipeline).ReleaseAndGetAddressOf())));

	}

	//パイプラインステートの生成
	{
		//シェーダーオブジェクトの生成
#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ComPtr<ID3DBlob> uiVS;
		ComPtr<ID3DBlob> uiPS;
		ComPtr<ID3DBlob> errorBlob = nullptr;

		//UI用バーテックスシェーダー
		checkErrorBloob(D3DCompileFromFile(L"UI_VertexShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", compileFlags, 0, &uiVS, &errorBlob), errorBlob);

		checkErrorBloob(D3DCompileFromFile(L"UI_PixelShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", compileFlags, 0, &uiPS, &errorBlob), errorBlob);

		//ペラポリゴン用
		D3D12_INPUT_ELEMENT_DESC layout[] =
		{
			{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},			//頂点毎の座標
			{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},			//モデルの法線
			{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},			//テクスチャ座標（UV）
		};

		//UIパイプラインステートオブジェクト
		pipelineStateObject_->CreatePipelineStateUi(layout, _countof(layout), RootSignatureFrom(uiPipeline), uiVS, uiPS, PipelineStateFrom(uiPipeline));

	}


}

void UiManager::CreateUiRTV()
{

	createResource_.reset(new CreateResource(DeviceFrom(uiRenderContext_.commandContext), uiRenderContext_.rtvHeaps));
	auto& bbuff = uiRenderContext_.renderTargets[0];
	auto resDesc = bbuff->GetDesc();	//もともと使用しているバッファの情報取得

	float clsClr[4] = { 0.2f,0.2f,0.2f,1.0f }; //レンダリング時のクリア値と同じ値

	resUi = createResource_->CreateMPR_Resource(D3D12_HEAP_TYPE_DEFAULT, clsClr, DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_STATE_RENDER_TARGET, resDesc);


	auto heapDesc = uiRenderContext_.rtvHeaps->GetDesc();
	heapDesc.NumDescriptors = 1;	//UI用RTV１つ
	ThrowIfFailed(DeviceFrom(uiRenderContext_.commandContext)->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(uiRtvHeap.ReleaseAndGetAddressOf())), L"\n_peraRTVHeap作成に失敗しました");

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	auto handle = uiRtvHeap->GetCPUDescriptorHandleForHeapStart();

	createResource_->CreateRTV_View(
		resUi.Get(),
		rtvDesc,
		handle
	);

}

//UI描画前の下準備
void UiManager::preDrawUI()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resUi.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	GraphicsListFrom(uiRenderContext_.commandContext)->ResourceBarrier(1, &barrier);

	auto incSize = DeviceFrom(uiRenderContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UINT offset = 0;
	auto rtvBaceHandle = uiRtvHeap->GetCPUDescriptorHandleForHeapStart();
	auto inc = DeviceFrom(uiRenderContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	GraphicsListFrom(uiRenderContext_.commandContext)->OMSetRenderTargets(1, &rtvBaceHandle, false, nullptr);

	//クリアカラー
	float clsClr[4] = { 0.2f,0.2f,0.2f,1.0f };
	GraphicsListFrom(uiRenderContext_.commandContext)->ClearRenderTargetView(rtvBaceHandle, clsClr, 0, nullptr);

}

void UiManager::postDrawUI()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resUi.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	GraphicsListFrom(uiRenderContext_.commandContext)->ResourceBarrier(1, &barrier);

}


void UiManager::InitUiManager()
{
	CreateUiPipeline();
	//UI作成処理の初期化//////////
	CreateUiHeap();

	CreateUiRTV();
	////////////////////////////

}

void UiManager::AddScreen(const UIScreenKey& key, std::unique_ptr<IUIScreen> screen)
{
	screens_[key] = std::move(screen);
}



void UiManager::ChangeScreen(const UIScreenKey& nextScreen)
{


	if (currentKey_ == nextScreen) return;

	//現在実行中のUIがあればExitを呼ぶ
	if (currentScreen_) {
		currentScreen_->Exit();
	}

	//次の画面が空文字列なら、currentScreen_をnullptrにして終了
	if (nextScreen.empty()) {
		currentKey_.clear();
		currentScreen_ = nullptr;
		return;
	}

	//次の画面がscreens_に存在するか確認
	auto it = screens_.find(nextScreen);
	if (it == screens_.end() || !it->second) {
		currentKey_.clear();
		currentScreen_ = nullptr;
		return;
	}

	//次の画面を currentScreen_ にセット
	currentKey_ = nextScreen;

	currentScreen_ = screens_[nextScreen].get();

	//次の画面の Initialize() を呼ぶ
	if (currentScreen_) {
		currentScreen_->Initialize();
	}
}




void UiManager::UpdateUI()
{
	if (!currentScreen_) {
		return;
	}

	//UI更新処理で、UIが遷移したら
	if (currentKey_ != currentScreen_->GetChangeUiScreenKey()) {
		ChangeScreen(currentScreen_->GetChangeUiScreenKey());
		return;
	}

	if (currentScreen_) {
		currentScreen_->Update();
	}
}


//UI描画処理
void UiManager::DrawUI()
{
	if (!currentScreen_) {
		return;
	}

	if (currentScreen_) {
		currentScreen_->Draw();
	}

	//UI文字描画処理
	DrawDirectWrite();
}

void UiManager::DrawDirectWrite()
{
	if (!currentScreen_) {
		return;
	}
	//対象のUI必ずDirectWrite更新処理実行する
	//更新しないと文字描画できない
	if (currentScreen_) {
		currentScreen_->DrawUIText();
	}
}

UIHitResult UiManager::IsHoveringUI()
{
	if (!currentScreen_) {
		return {};
	}
	return currentScreen_->IsHoveringUI();
}

IUIScreen* UiManager::GetUIScreen(const UIScreenKey& key)
{
	auto it = screens_.find(key);
	if (it == screens_.end()) {
		return nullptr;
	}

	return it->second.get();
}