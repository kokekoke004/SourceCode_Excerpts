#pragma once
#include<d3d12.h>
#include"d3dx12.h"
#include<DirectXMath.h>
#include<unordered_map>
#include<string>
#include <memory>
#include<chrono>
#include<array>
#include<D3Dcompiler.h>


#pragma comment(lib,"libfbxsdk-md.lib")
#pragma comment(lib,"libxml2-md.lib")
#pragma comment(lib,"zlib-md.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")


#include"helper.h"
#include"DXAppUtillity.h"
#include"Uiutillity.h"
#include"IUIScreen.h"


class DirectWrite_2D;
class CreateResource;
class MouseInput;
class PipelineStateObject;
class TextInputManager;
class Screenshot;
class UIBuildSystem;


struct UIRenderContext
{
	CommandContext& commandContext;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& rtvHeaps;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& renderTargets;
	const ViewportScissor& viewportScissor;

	UIRenderContext(
		CommandContext& commandContext_,
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& rtvHeaps_,
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& renderTargets_,
		const ViewportScissor& viewportScissor_
	)
		: commandContext(commandContext_)
		, rtvHeaps(rtvHeaps_)
		, renderTargets(renderTargets_)
		, viewportScissor(viewportScissor_)
	{
	}

};

class UiManager
{
public:
	UiManager(UIRenderContext uiRenderContext);

	//UI画面の追加
	void AddScreen(const UIScreenKey& key, std::unique_ptr<IUIScreen> screen);
	//UIクラスの初期化
	void InitUiManager();
	//UI描画前の下準備
	void preDrawUI();
	//UI描画処理
	void DrawUI();
	//UI描画後の処理
	void postDrawUI();
	//DirectWrite描画処理
	void DrawDirectWrite();
	//UIアップデート処理
	void UpdateUI();
	//UI遷移
	//nextScreen : 遷移先UIID
	void ChangeScreen(const UIScreenKey& nextScreen);

	//UI上にカーソルがあるかどうか(false = UI上にカーソルない　/ true = UI上にカーソルある)
	UIHitResult IsHoveringUI();

	const UIScreenKey& GetCurrentKey() const
	{
		return currentKey_;
	}

	IUIScreen* GetCurrentScreen()
	{
		return currentScreen_;
	}

	ComPtr<ID3D12DescriptorHeap>& GetUIHeap()
	{
		return uiHeap;
	}

	const ComPtr<ID3D12DescriptorHeap>& GetUIHeap() const
	{
		return uiHeap;
	}



	std::unordered_map < std::wstring, int>& GetHiHeapIdx()
	{
		return uiHeapIdx;
	}

	const std::unordered_map < std::wstring, int>& GetHiHeapIdx() const 
	{
		return uiHeapIdx;
	}

	const UIPipeline& GetUIPipeline() const
	{
		return uiPipeline;
	}


	//UI画面を特定のクラスにキャストして取得するためのテンプレート関数
	template<class T>
	T* GetUIScreenAs(const UIScreenKey& key)
	{
		return dynamic_cast<T*>(GetUIScreen(key));
	}


private:
	std::unordered_map<UIScreenKey, std::unique_ptr<IUIScreen>> screens_;
	IUIScreen* currentScreen_ = nullptr;
	UIScreenKey currentKey_;
	UIRenderContext uiRenderContext_;

	UIPipeline uiPipeline;

	std::unique_ptr<PipelineStateObject> pipelineStateObject_;
	std::unique_ptr<CreateResource> createResource_;
	//CreateResource& createResource;


	CD3DX12_VIEWPORT viewport_;	//ビューポート
	CD3DX12_RECT scissorrect_;	//シザー矩形

	ComPtr<ID3D12DescriptorHeap> uiRtvHeap;
	ComPtr<ID3D12DescriptorHeap> uiHeap;
	ComPtr<ID3D12Resource> resUi;
	//キーUIの名前
	std::unordered_map < std::wstring, int> uiHeapIdx;


	//const CharacterManagers& characterMgrs;

	//UIで使用するヒープ作成
	void CreateUiHeap();
	void CreateUiRTV();
	void CreateUiPipeline();

	IUIScreen* GetUIScreen(const UIScreenKey& key);

};