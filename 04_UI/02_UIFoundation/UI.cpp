#include"UI.h"
#include"CreateResource.h"
#include"LoadTexture.h"
#include"DirectWrite_2D.h"
#include"MouseInput.h"
#include"UiEventManager.h"

#include"VertexBufferUtility.h"
#include"VertexBufferManager.h"
#include"InstanceBufferManager.h"


using namespace DirectX;
using namespace std;

UI::UI(UIService uiService,
	UIContext uiContext)
	:uiService_(uiService), uiContext_(uiContext)
{
	uiEventMgr.reset(new UiEventManager(uiService_.mouseInput));
	incSize = DeviceFrom(uiContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	sizeCb = alignmentSize(sizeof(UiDrawPos));
	//ウィンドウの高さと幅を求める
	auto hwnd = HwndFrom(uiContext_.commandContext);

	win.width = RenderSizeFrom(uiContext_.commandContext).windowPixWidth;
	win.height = RenderSizeFrom(uiContext_.commandContext).windowPixHeight;


	vertexBufferMgr.reset(new VertexBufferManager(uiContext_.commandContext, GetHeap()));
	createResource.reset(new CreateResource(DeviceFrom(uiContext_.commandContext), this->GetHeap()));
	instanceBufferManager.reset(new InstanceBufferManager(createResource));
	loadTex.reset(new LoadTex(DeviceFrom(uiContext_.commandContext)));

}

UI::~UI()
{

}

void UI::UpdateCommon()
{
	auto uiName = Getter_drawUiName();
	std::wstring keyUiName;

	Vec2 mousePos = GetMouseInput().GetMousePos();
	Vec2 movDis = GetMouseInput().GetMouseVelocity();

	//UI上にカーソルがあるかどうかの判定結果をリセット
	//カーソル上に前フレームあったとき、ここで一回初期化
	if (GetUiHoverHitResult().isHit) {
		GetUiHoverHitResult() = {};
	}


	for (auto uiPartsName : uiName) {
		keyUiName = uiPartsName;
		//カーソルがUI上にあるかどうか
		FindUIElementAtCursor(keyUiName, mousePos);

		BuildInstanceData(keyUiName);
	}

}

void UI::DrawCommon()
{
	std::wstring keyUiName;

	auto uiName = Getter_drawUiName();

	GraphicsListFrom(GetCommandContext())->RSSetViewports(1, &ViewportFrom(GetViewportScissor()));
	//commandList_->RSSetScissorRects(1, &scissorrect_);	//シザー矩形

	//UI上にカーソルがあるかどうかの判定結果をリセット
	//カーソル上に前フレームあったとき、ここで一回初期化
	if (GetUiHoverHitResult().isHit) {
		GetUiHoverHitResult() = {};
	}

	//UIの描画順をソート
	SortUIDrawOrder();

	for (auto uiPartsName : uiDrawOrderList_) {
		keyUiName = uiPartsName.keyUiName;
		if (GetterDrawNum(keyUiName) <= 0) {
			continue;
		}
		CD3DX12_RECT scissorrectUI_ = Getter_scissorRect(keyUiName);
		//CommandListFrom(commandContext)->RSSetViewports(1, &viewport_);
		GraphicsListFrom(GetCommandContext())->RSSetScissorRects(1, &scissorrectUI_);	//シザー矩形


		GraphicsListFrom(GetCommandContext())->SetGraphicsRootSignature(RootSignatureFrom(GetUiPipe()).Get());	//SetGraphicsRootDescriptorTableこの前で呼び出す
		//uiTex.second->MoveDrawPoint(keyUiName, 0, mouseInfo.movDis.X, mouseInfo.movDis.Y);

		//ヒープをセット
		ID3D12DescriptorHeap* ppHeaps[] = { GetHeap().Get() };
		GraphicsListFrom(GetCommandContext())->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		auto handle = GetHeap()->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += DeviceFrom(GetCommandContext())->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * (UI_ALL_NUM_CBV_SRV)*Getter_heapIdx(keyUiName);

		GraphicsListFrom(GetCommandContext())->SetGraphicsRootDescriptorTable(0, handle);
		handle.ptr += DeviceFrom(GetCommandContext())->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		GraphicsListFrom(GetCommandContext())->SetPipelineState(PipelineStateFrom(GetUiPipe()).Get());

		GraphicsListFrom(GetCommandContext())->IASetPrimitiveTopology(
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		D3D12_VERTEX_BUFFER_VIEW uiVbv = Getter_uiVbv(keyUiName);
		GraphicsListFrom(GetCommandContext())->IASetVertexBuffers(0, 1, &uiVbv);
		D3D12_INDEX_BUFFER_VIEW uiIbv = Getter_uiIbv(keyUiName);
		GraphicsListFrom(GetCommandContext())->IASetIndexBuffer(&uiIbv);
		GraphicsListFrom(GetCommandContext())->DrawIndexedInstanced(6, GetterDrawNum(keyUiName), 0, 0, 0);
	}

}


void UI::CreateUi(std::wstring keyUiName, std::wstring keyUiTexName, bool flagDrawTexSize, bool flagAnim, float x, float y, float z, float width, float height, float rote, UIDrawLayer drawLayer)
{
	UiInfo uiInf;
	if (FindKey(keyUiName, uiContext_.uiHeapIdx)) {
		printf("\n関数名：CreateUi　：　uiHeapIdxに登録するキー名が被っています");
		ThrrowError("関数名：CreateUi　：　uiHeapIdxに登録するキー名が被っています");
	}
	uiContext_.uiHeapIdx[keyUiName] = uiContext_.uiHeapIdx.size();
	uiInf.x = x;
	uiInf.y = y;
	uiInf.z = z;

	printf("\nvertexBufferMgr");
	assert(vertexBufferMgr);
	printf("\nheap_");
	assert(GetHeap());
	printf("\ncreateResource");
	assert(createResource);
	printf("\nOK");

	uiKey[keyUiName].keyUiParts = keyUiName;
	uiKey[keyUiName].keyUiTexPass = keyUiTexName;
	loadUiTex(keyUiName,keyUiTexName);


	//アニメーションの処理があれば////
	////////////////////////////////
	uiInfo[keyUiName] = uiInf;

	printf("\nテクスチャの大きさで描画するかどうか");
	//テクスチャの大きさで描画するかどうか
	if (flagDrawTexSize) {
		width = uiTexInfo.width;
		height = uiTexInfo.height;
	}

	vertexBufferMgr->CreateVertexBuffer(resUi[keyUiName], indexRes[keyUiName], uiVbv[keyUiName], uiIbv[keyUiName],VertexBufferType::UI,width,height,z);
	CreateUiVertexBuffer(keyUiName,flagDrawTexSize, x, y, z, width, height, rote);
	printf("\nheapAdd");
	D3D12_CPU_DESCRIPTOR_HANDLE heapAdd = GetHeap()->GetCPUDescriptorHandleForHeapStart();
	auto inc = DeviceFrom(uiContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	heapAdd.ptr+= inc * (UI_ALL_NUM_CBV_SRV) *uiContext_.uiHeapIdx[keyUiName];
	//CBV
	createResource->CreateConstantBufferView(cbUi[keyUiName], heapAdd, 0, sizeCb);
	//SRV
	heapAdd.ptr += inc;
	//テクスチャロード(SRV)
	createResource->CreateShaderResourceView(uiTex[uiKey[keyUiName].keyUiTexPass].Get(), heapAdd);

	RegisterUIDrawOrder(keyUiName, drawLayer);

}

void UI::CreateUiVertexBuffer(std::wstring keyUiName, bool flagDrawTexSize, float x, float y, float z, float width, float height, float rote)
{

	if (flagDrawTexSize) {
		width = uiTexInfo.width;
		height = uiTexInfo.height;
	}

	uiInfo[keyUiName].width = width;
	uiInfo[keyUiName].height = height;
	uiInfo[keyUiName].z = z;



	uiDrawPos[keyUiName].rote = XMMatrixTranspose(XMMatrixRotationZ(rote));

	//描画する座標などの情報を持たせたコンスタントバッファ
	cbUi[keyUiName] = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeCb, D3D12_RESOURCE_STATE_GENERIC_READ);
	//ピクセル座標をスクリーン座標へ変換する行列の作成
	uiInfo[keyUiName].windowScale = XMMatrixTranspose(XMMatrixScaling(
		2.0f / static_cast<float>(win.width),
		2.0f / static_cast<float>(win.height),
		1.f
	));



	uiDrawPos[keyUiName].pos.x = x;
	uiDrawPos[keyUiName].pos.y = y;
	uiDrawPos[keyUiName].pos.z = z;
	uiDrawPos[keyUiName].pos.w = 0.0f;


	//これで、UIの左上を中心としている
	SetUiCenter(uiDrawPos[keyUiName].pos.x, uiDrawPos[keyUiName].pos.y);

	uiDrawPos[keyUiName].orthoMatrix = (uiInfo[keyUiName].windowScale);


	UiDrawPos* cb = nullptr;
	cbUi[keyUiName]->Map(0, nullptr, (void**)&cb);
	memcpy(cb,&uiDrawPos[keyUiName],sizeof(UiDrawPos));
	cbUi[keyUiName]->Unmap(0, nullptr);

}


void UI::loadUiTex(wstring keyUiName,wstring texPass)
{

	printf("\nloadUiTex実行");


	for (auto& keyUi : uiKey) {
		//もし指定されたキーで、リソースを生成していなかったら

		if (keyUi.first==keyUiName&& uiTex[texPass].Get() == nullptr) {	//リソースが作成されていなかったらnullptrを返却する
			printf("\n%s", keyUi.first.c_str());
			printf("\nテクスチャパスをUTF8から16へ");
			uiTex[texPass] = loadTex->loadTex(texPass);
			printf("\nテクスチャパスをUTF8から16変換完了");
			keyUi.second.keyUiTexPass = texPass;
			break;
		}
		else if(keyUi.first == keyUiName && uiTex[texPass].Get() != nullptr) { 	//UIテクスチャをすでにウ重きんでいるとき
			printf("\nすでにテクスチャパスは登録してあります");
			ThrowMessageWstr(L"\nすでにテクスチャパスは登録してあります");
			uiKey[keyUiName].keyUiTexPass = texPass;
			break;
		}

	}
	D3D12_RESOURCE_DESC desc = uiTex[texPass]->GetDesc();
	UINT width = static_cast<UINT>(desc.Width);
	UINT height = desc.Height;

	printf("\nUIテクスチャの横の大きさ : %u", width);
	printf("\nUIテクスチャの縦の大きさ : %u", height);

	ThrowMessageWstr(L"\nUIテクスチャの横の大きさ : " + to_wstring(width));
	ThrowMessageWstr(L"\nUIテクスチャの縦の大きさ : " + to_wstring(height));

	uiTexInfo.height = height;
	uiTexInfo.width = width;
}

void UI::SetUiCenter(float& x, float& y)
{
	x = -1.f + (x * 2) / win.width;
	y = 1.f - (y * 2) / win.height;
}

void UI::ChangeTextDrawFlag(std::wstring keyUiPartsName, int idxText, bool flagDraw)
{
	if (textInfo.find(keyUiPartsName) != textInfo.end() && idxText < static_cast<int>(textInfo[keyUiPartsName].size())) {
		textInfo[keyUiPartsName][idxText].flagDraw = flagDraw;
	}

}

void UI::ChangeDrawFlag(std::wstring keyUiPartsName,int idx, bool flagDraw)
{

	originalInstancePos[keyUiPartsName][idx].flagDraw = flagDraw;
	if (textInfo.find(keyUiPartsName) != textInfo.end() && idx < static_cast<int>(textInfo[keyUiPartsName].size())) {
		textInfo[keyUiPartsName][idx].flagDraw = flagDraw;
	}

}

void UI::ExtractUITex(std::wstring keyUiPartsName, std::wstring keyNewUiPartsName, bool flagNoClip, DrawSizeType drawSizeType, float width, float height, float x, float y,float setSizeWidth,float setSizeHeight, UIDrawLayer drawLayer)
{
	if (keyUiPartsName == keyNewUiPartsName) {
		return;
	}
	uiKey[keyNewUiPartsName].keyUiTexPass = uiKey[keyUiPartsName].keyUiTexPass;

	if (FindKey(keyNewUiPartsName, uiContext_.uiHeapIdx) == true) {
		wprintf(L"\nkeyNewUiPartsName : %ls ", keyNewUiPartsName.c_str());
		printf("関数名：ExtractUITex　：　uiHeapIdxに登録するキー名が被っています");
		ThrrowError("関数名：ExtractUITex　：　uiHeapIdxに登録するキー名が被っています");
	}
	else {
		//新しいUIパーツのヒープのインデックスを登録する
		uiContext_.uiHeapIdx[keyNewUiPartsName] = uiContext_.uiHeapIdx.size();
	}


	uiOriginalData[keyUiPartsName] = keyNewUiPartsName;
	//描画する座標などの情報を持たせたコンスタントバッファ
	cbUi[keyNewUiPartsName] = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeCb, D3D12_RESOURCE_STATE_GENERIC_READ);
	//頂点バッファで使用
	//resUi[keyNewUiPartsName] = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, size_uiV, D3D12_RESOURCE_STATE_GENERIC_READ);
	uiTex[keyNewUiPartsName] = uiTex[uiKey[keyUiPartsName].keyUiTexPass];
	uiDrawPos[keyNewUiPartsName].uvPos =ExtractTextureRegion(uiTex[uiKey[keyUiPartsName].keyUiTexPass], flagNoClip, width, height, x, y);
	uiDrawPos[keyNewUiPartsName].orthoMatrix = (uiInfo[keyUiPartsName].windowScale);
	uiDrawPos[keyNewUiPartsName].rote = uiDrawPos[keyUiPartsName].rote;
	UiDrawPos* cb = nullptr;
	cbUi[keyNewUiPartsName]->Map(0, nullptr, (void**)&cb);
	memcpy(cb, &uiDrawPos[keyNewUiPartsName], sizeof(UiDrawPos));
	cbUi[keyNewUiPartsName]->Unmap(0, nullptr);

	UiVertex* mapped = nullptr;
	//UIテクスチャの縦、横幅
	float texWidth = uiTexInfo.width;
	float texHeight = uiTexInfo.height;

	float setUiSizeWidth = 0;
	float setUiSizeHeight = 0;
	switch (drawSizeType)
	{
	case DRAW_TEX_SIZE:
		vertexBufferMgr->CreateVertexBuffer(resUi[keyNewUiPartsName], indexRes[keyNewUiPartsName], uiVbv[keyNewUiPartsName], uiIbv[keyNewUiPartsName], VertexBufferType::UI,
			texWidth, texHeight, uiInfo[keyUiPartsName].z);

		setUiSizeWidth = texWidth;
		setUiSizeHeight = texHeight;
		break;
	case DRAW_ORIGINAL_SIZE:
		//CreateUI時のサイズで描画する
		//map処理//頂点バッファ
		uiInfo[keyNewUiPartsName].height = uiInfo[keyUiPartsName].height;
		uiInfo[keyNewUiPartsName].width = uiInfo[keyUiPartsName].width;
		vertexBufferMgr->CreateVertexBuffer(resUi[keyNewUiPartsName], indexRes[keyNewUiPartsName], uiVbv[keyNewUiPartsName], uiIbv[keyNewUiPartsName], VertexBufferType::UI,
			uiInfo[keyNewUiPartsName].width, uiInfo[keyNewUiPartsName].height, uiInfo[keyUiPartsName].z);

		setUiSizeWidth = uiInfo[keyNewUiPartsName].width;
		setUiSizeHeight = uiInfo[keyNewUiPartsName].height;


		break;
	case DRAW_SET_SIZE:
		//新しくUIのサイズをセットする
		uiInfo[keyNewUiPartsName].height = setSizeHeight;
		uiInfo[keyNewUiPartsName].width = setSizeWidth;

		setUiSizeWidth = setSizeWidth;
		setUiSizeHeight = setSizeHeight;


		setSizeHeight;
		setSizeWidth;

		vertexBufferMgr->CreateVertexBuffer(resUi[keyNewUiPartsName], indexRes[keyNewUiPartsName], uiVbv[keyNewUiPartsName], uiIbv[keyNewUiPartsName], VertexBufferType::UI,
			setSizeWidth, setSizeHeight, uiInfo[keyUiPartsName].z);



	}

	partLayouts_[keyNewUiPartsName] = { setUiSizeWidth, setUiSizeHeight };

	//CBVやテクスチャのSRV作成
	D3D12_CPU_DESCRIPTOR_HANDLE heapAdd = GetHeap()->GetCPUDescriptorHandleForHeapStart();
	auto inc = DeviceFrom(uiContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	heapAdd.ptr += inc * (UI_ALL_NUM_CBV_SRV)*uiContext_.uiHeapIdx[keyNewUiPartsName];
	//CBV
	createResource->CreateConstantBufferView(cbUi[keyNewUiPartsName], heapAdd, 0, sizeCb);
	//SRV
	heapAdd.ptr += inc;
	//テクスチャロード(SRV)
	createResource->CreateShaderResourceView(uiTex[uiKey[keyUiPartsName].keyUiTexPass].Get(), heapAdd);

	RegisterUIDrawOrder(keyNewUiPartsName, drawLayer);

}

void UI::RegisterUIDrawOrder(const std::wstring& keyUiName, UIDrawLayer drawLayer)
{
	UIDrawOrder order;
	order.keyUiName = keyUiName;
	order.drawLayer = drawLayer;
	order.addOrder = uiDrawOrderList_.size();

	uiDrawOrderList_.push_back(order);
}

void UI::SortUIDrawOrder()
{
	//UIDrawLayerの値が小さい順にソートする
	std::stable_sort(
		uiDrawOrderList_.begin(),
		uiDrawOrderList_.end(),
		[](const UIDrawOrder& a, const UIDrawOrder& b)
		{
			const int aLayer = static_cast<int>(a.drawLayer);
			const int bLayer = static_cast<int>(b.drawLayer);

			if (aLayer != bLayer)
			{
				return aLayer < bLayer;
			}

			return a.addOrder < b.addOrder;
		}
	);
}

void UI::DrawDirectWrite()
{
	string keyUiName;
	DrawUiTex();

}

int UI::PostDrawUi(std::wstring keyUiPartsName,UiPartMeta uiPartMeta, float x, float y, float z, float rote, bool flagDraw, bool flagAnimation,bool flagWriteText)
{
	int idxOrg = originalInstancePos[keyUiPartsName].size();
	int idxIns = instanceInfo[keyUiPartsName].size();

	if (idxOrg > 0) {
		idxOrg--;
	}

	if (idxIns > 0) {
		idxIns--;
	}



	uiInfo[keyUiPartsName].x = x;
	uiInfo[keyUiPartsName].y = y;
	//シザーレクト設定
	SetScissorRect(keyUiPartsName);
	float roteRadi = rote * XM_PI / 180;

	//インスタンス描画用NDC変換前の様々なデータを扱ってるuiFixName
	OriginalInstancePos oIpos;
	oIpos.worldPos.x = x;
	oIpos.worldPos.y = y;
	oIpos.worldPos.z = z;
	oIpos.worldPos.w = 0;
	oIpos.uv = uiDrawPos[keyUiPartsName].uvPos;
	oIpos.uiPartMeta = uiPartMeta;
	oIpos.flagText = flagWriteText;
	oIpos.uiHeight = uiInfo[keyUiPartsName].height;
	oIpos.uiWidth = uiInfo[keyUiPartsName].width;
	oIpos.scissorRectsSizeHeight = win.height;
	oIpos.scissorRectsSizeWidth = win.width;
	oIpos.flagDraw = flagDraw;

	XMStoreFloat4x4(&oIpos.rote, XMMatrixTranspose(XMMatrixRotationZ(roteRadi)));
	originalInstancePos[keyUiPartsName].emplace_back(oIpos);

	if (flagAnimation) {
		StartUiAnim(keyUiPartsName, idxOrg);
		uiInfo[keyUiPartsName].flagAnim = true;
	}

	auto it = std::find(drawUiName.begin(), drawUiName.end(), keyUiPartsName);
	if (it == drawUiName.end()) {
		drawUiName.emplace_back(keyUiPartsName);
	}
	//インスタンス描画用に描画するUIの数をカウントで増やす
	uiInfo[keyUiPartsName].drawNum++;

	//UIのインスタンス描画で使用する
	//DrawUiを呼び出す毎にリソースを生成し直し、インスタンス描画に対応できるようにしている
	InstanceUi instance;
	instanceBufferManager->MakeInstancingResource(keyUiPartsName, instance);

	D3D12_CPU_DESCRIPTOR_HANDLE heapAdd = GetHeap()->GetCPUDescriptorHandleForHeapStart();
	auto inc = DeviceFrom(uiContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	heapAdd.ptr += inc * (UI_ALL_NUM_CBV_SRV)*uiContext_.uiHeapIdx[keyUiPartsName];
	//インスタンス描画で使用するSRV作成
	//CBV
	heapAdd.ptr += inc;
	//SRV
	instanceBufferManager->MakeInstancingResourceView(keyUiPartsName, instance,heapAdd,0);
	int size=sizeof(InstanceUi);
	SetUiCenter(x, y);
	//インスタンス描画用変数に座標やUV座標を渡す
	instance.world = XMFLOAT4(x, y, z, 0.0f);
	instance.uv = uiDrawPos[keyUiPartsName].uvPos;
	XMStoreFloat4x4(&instance.rote, XMMatrixTranspose(XMMatrixRotationZ(roteRadi)));

	//instance.rote = oIpos.rote;
	instanceInfo[keyUiPartsName].emplace_back(instance);

	return uiInfo[keyUiPartsName].drawNum-1;
}



void UI::UiAnim(std::wstring keyUiName, int idx, bool flagChangeUiSize,float width, float height, float x, float y, float changeTime)
{
	wstring keyNewUiName = keyUiName;
	//map登録されていないとき""これが入る＝＝新しく登録したUIではない
	if (uiOriginalData[keyUiName] != L"") {
		//mapに登録されていれば、元のUIの名前をkeyUiNameに代入する
		keyUiName = uiOriginalData[keyUiName];
	}
	if (uiInfo[keyUiName].flagExtractPol && flagChangeUiSize) {
		//テクスチャの大きさをポリゴンの大きさに合わせる
		ExtractUITex(keyUiName, keyNewUiName,true, DRAW_ORIGINAL_SIZE,width, height, x, y);
		uiInfo[keyUiName].flagExtractPol = false;
	}
	double time = timeGetTime()/1000 - originalInstancePos[keyUiName][idx].animTime;
	if (originalInstancePos[keyUiName][idx].flagAnim && time >= changeTime) {
		StartUiAnim(keyUiName,idx);
		uiDrawPos[keyUiName].uvPos = ExtractTextureRegion(uiTex[uiKey[keyUiName].keyUiTexPass], true, width, height, uiInfo[keyUiName].uvX, uiInfo[keyUiName].uvY);
		//アニメーションのUV座標を更新
		for (auto& animUv : instanceInfo[keyUiName]) {
			animUv.uv = ExtractTextureRegion(uiTex[uiKey[keyUiName].keyUiTexPass], true, width, height, uiInfo[keyUiName].uvX, uiInfo[keyUiName].uvY);
		}
		uiInfo[keyUiName].uvX += width;
		if (uiInfo[keyUiName].uvX >= uiTexInfo.width) {
			uiInfo[keyUiName].uvY += height;
			uiInfo[keyUiName].uvX = 0;
		}
		UiDrawPos* cb = nullptr;
		cbUi[keyUiName]->Map(0, nullptr, (void**)&cb);
		memcpy(cb, &uiDrawPos[keyUiName], sizeof(UiDrawPos));
		cbUi[keyUiName]->Unmap(0, nullptr);

	}
}

float UI::CalculateCenteredTextYOffset(const std::string& fontKey, float uiHeight)
{
	const auto& textRenderer = GetTextRenderer();
	const auto& textFont = textRenderer.GettextFormatMap(fontKey);
	const auto& textFontSize = textFont.Get()->GetFontSize();
	return (uiHeight - textFontSize) * 0.5f;
}

float UI::CalculateCenteredTextXOffset(const std::string& fontKey, const std::wstring& text, float uiSize)
{
	auto& textRenderer = GetTextRenderer();
	float textWidth = textRenderer.GetTextWidth(text, fontKey);
	return (uiSize - textWidth) * 0.5f;
}

float UI::GetFontSize(const std::string& textFont) const
{
	const auto& textRender = GetTextRenderer();
	const auto& fontData = textRender.GettextFormatMap(textFont);

	return fontData.Get()->GetFontSize();
}

float UI::CalculateTextHorizontalOffset(int textNum, float textSize) const
{
	return  static_cast<float>(textNum) * textSize;
}

void UI::StartUiAnim(const std::wstring& keyUiName,UINT idx)
{
	originalInstancePos[keyUiName][idx].flagAnim = true;
	originalInstancePos[keyUiName][idx].animTime = timeGetTime() / 1000;
}

void UI::DrawUiTex()
{

	for (auto& texInf : textInfo) {
		int count = 0;
		wstring keyUiName;
		for (auto& tex : texInf.second) {
			keyUiName = texInf.first;
			if (tex.flagDraw == false) {
				count++;
				continue;
			}

			uiService_.textRenderer.DrawTextOnWindow(tex.textFormatKey, tex.textColorKey, tex.text, textInfo[keyUiName][count].x, textInfo[keyUiName][count].y,
				textInfo[keyUiName][count].texBoxWidth, textInfo[keyUiName][count].texBoxHeight, textInfo[keyUiName][count].tX, textInfo[keyUiName][count].tY);
			count++;
		}
	}

}

void UI::MoveDrawPoint(const std::wstring& uiName,const std::shared_ptr<UiRelation>& node, float x, float y)
{
	if (!node) {
		flagBackGround = false;
		return;
	}

	wstring keyUiParts = registerUiKeyName[uiName][node->nodeName].uiOriginalName;
	//printf("\nマウス移動量差分 Y : %f", y);



	uiInfo[keyUiParts].x = uiDrawPos[keyUiParts].pos.x;
	uiInfo[keyUiParts].y = uiDrawPos[keyUiParts].pos.y;
	uiInfo[keyUiParts].z = uiDrawPos[keyUiParts].pos.z;

	int notMove = 0;

	//UI移動開始時の各座標を保持
	if (!uiStartPos[keyUiParts].flagHaveUiStartPos) {
		for (auto orgPos : originalInstancePos[keyUiParts]) {
			uiStartPos[keyUiParts].uiStartPos.emplace_back(orgPos.worldPos);
		}
		uiStartPos[keyUiParts].flagHaveUiStartPos = true;
	}

	//UI移動開始時の各座標を保持（テキストの座標）
	if (!uiStartPos[keyUiParts].flagHaveUiTextStartPos) {
		for (auto& uiText : textInfo[keyUiParts]) {
			XMFLOAT2 texWpos = XMFLOAT2(uiText.tX, uiText.tY);
			uiStartPos[keyUiParts].uiTextStartPos.emplace_back(texWpos);
		}
		uiStartPos[keyUiParts].flagHaveUiTextStartPos = true;
	}


	//インスタンス描画の座標もここで更新
	switch (uiNodeType[uiName]) {
	case UI_NODE_NONE_CONF:
		MoveUi_UI_NODE_NONE_CONF(uiName, node, x, y);
		break;
	case UI_NODE_SLIDER_VERTICAL:
		MoveUiSlider(uiName, node,x,y);
		if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
			return;
		}
		break;
	case UI_NODE_SLIDER_HORIZON:

		break;
	case UI_NODE_SLIDER_VERTICAL_ONLY_TEXT:
	{
		MoveUiSliderOnlyText(uiName, node, x, y);
		if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
			return;
		}
		break;
	}
	default:
		break;
	}


	for (auto uinode : node->child) {
		MoveDrawPoint(uiName,uinode, x, y);
	}

}

void UI::MoveUiSliderOnlyText(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node, float& mX, float& mY)
{
	wstring keyUiParts = registerUiKeyName[uiName][node->nodeName].uiOriginalName;
	InstanceUi instance;

	float x = 0;
	float y = 0;
	float z = 0;

	float tX = 0;
	float tY = 0;
	float tZ = 0;

	bool flagUpDateTextPos = false;

	int idx = 0;


	int idxUiStrPs = 0;
	for (auto& pos : originalInstancePos[keyUiParts]) {
		//UI
		x = uiStartPos[keyUiParts].uiStartPos[idxUiStrPs].x;
		y = uiStartPos[keyUiParts].uiStartPos[idxUiStrPs].y;
		z = pos.worldPos.z;
		//UIのテキスト
		if (uiStartPos[keyUiParts].uiTextStartPos.size() > idxUiStrPs) {
			flagUpDateTextPos = true;
			tX = uiStartPos[keyUiParts].uiTextStartPos[idxUiStrPs].x;
			tY = uiStartPos[keyUiParts].uiTextStartPos[idxUiStrPs].y;

		}


		//デバッグ用
		//if (pos.drawUiPartsType == UI_SLIDER_VERTICAL) {
			//SetScissorRect(keyUiParts, false, pos.scissorRectsPosX, pos.scissorRectsPosY, pos.scissorRectsPosX + pos.scissorRectsSizeWidth, pos.scissorRectsPosY + pos.scissorRectsSizeHeight);
		//}
		//UIスクロールバーの移動処理
		float moveY = 0;
		if (pos.uiPartMeta.sliderRole == SLIDER_VERTICAL_SCROLLBAR) {
			if (y + mY < sliderInfo[uiName].scrollMin) {

				if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
					return;
				}

				float over = sliderInfo[uiName].scrollMin - y;

				y = sliderInfo[uiName].scrollMin;

				//スクロール限界 - 前回ループの座標で移動可能な差分を求めmYにくわえる
				mY = over;
				UpDateMoveInsPos(keyUiParts, pos, instance, x, y, z, mX, mY, idx, idxUiStrPs);



				for (auto uinode : node->child) {
					MoveDrawPoint(uiName, uinode, mX, over);
				}

				sliderInfo[uiName].flagTop = true;
				return;

				//ThrowMessageWstr(L"\nmY : " + to_wstring(mY));
			}
			else if (y + mY + sliderInfo[uiName].scrollbarSizeHeight > sliderInfo[uiName].scrollMax) {

				if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
					return;
				}

				float over = (sliderInfo[uiName].scrollMax - sliderInfo[uiName].scrollbarSizeHeight) - y;

				y = sliderInfo[uiName].scrollMax - sliderInfo[uiName].scrollbarSizeHeight;
				//スクロール限界 - 前回ループの座標で移動可能な差分を求めmYにくわえる
				mY = over;

				UpDateMoveInsPos(keyUiParts, pos, instance, x, y, z, mX, mY, idx, idxUiStrPs);


				for (auto uinode : node->child) {
					MoveDrawPoint(uiName, uinode, mX, over);
				}

				sliderInfo[uiName].flagUnder = true;

				return;

			}
			else {
				y += mY;
				//ThrowMessageWstr(L"\nscrollRatio : " + to_wstring(sliderInfo[uiName].scrollRatio));
				sliderInfo[uiName].flagTop = false;
				sliderInfo[uiName].flagUnder = false;

			}
		}
		else {
			//スライダーでスライドバー以外の移動速度
			if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
				return;
			}
			moveY = mY * sliderInfo[uiName].scrollRatioMovPixels;
		}

		UpDateMoveOnlyTextPos(uiName, keyUiParts, 0, moveY, tZ, idx, flagUpDateTextPos);
		UpDateMoveInsPos(keyUiParts, pos, instance, x, y, z, mX, mY, idx, idxUiStrPs);
	}


}


void UI::MoveUiSlider(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node,float& mX,float& mY)
{
	wstring keyUiParts = registerUiKeyName[uiName][node->nodeName].uiOriginalName;
	InstanceUi instance;

	float x = 0;
	float y = 0;
	float z = 0;


	int idx = 0;
	int idxUiStrPs = 0;

	for (auto& pos : originalInstancePos[keyUiParts]) {
		float addTX = 0.0f;
		float addTY = 0.0f;

		//UI
		x = uiStartPos[keyUiParts].uiStartPos[idxUiStrPs].x;
		y = uiStartPos[keyUiParts].uiStartPos[idxUiStrPs].y;
		z = pos.worldPos.z;

		//UIスクロールバーの移動処理
		if (pos.uiPartMeta.sliderRole == SLIDER_VERTICAL_SCROLLBAR) {
			if (y + mY < sliderInfo[uiName].scrollMin) {

				if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
					return;
				}

				float over = sliderInfo[uiName].scrollMin - y;
				y = sliderInfo[uiName].scrollMin;

				//スクロール限界 - 前回ループの座標で移動可能な差分を求めmYにくわえる
				mY = over;

				UpDateMoveInsPos(keyUiParts, pos, instance, x, y, z, mX, mY, idx, idxUiStrPs);
				for (auto uinode : node->child) {
					MoveDrawPoint(uiName, uinode, mX, mY);
				}


				sliderInfo[uiName].flagTop = true;
				return;

				//ThrowMessageWstr(L"\nmY : " + to_wstring(mY));
			}
			else if (y + mY + sliderInfo[uiName].scrollbarSizeHeight > sliderInfo[uiName].scrollMax) {

				if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
					return;
				}

				float over = (sliderInfo[uiName].scrollMax - sliderInfo[uiName].scrollbarSizeHeight) - y;

				y = sliderInfo[uiName].scrollMax- sliderInfo[uiName].scrollbarSizeHeight;
				//スクロール限界 - 前回ループの座標で移動可能な差分を求めmYにくわえる
				mY = over;

				UpDateMoveInsPos(keyUiParts, pos, instance, x, y, z, mX, mY, idx, idxUiStrPs);
				for (auto uinode : node->child) {
					MoveDrawPoint(uiName, uinode, mX, mY);
				}



				sliderInfo[uiName].flagUnder = true;

				return;

			}
			else {
				y += mY;
				//ThrowMessageWstr(L"\nscrollRatio : " + to_wstring(sliderInfo[uiName].scrollRatio));
				sliderInfo[uiName].flagTop = false;
				sliderInfo[uiName].flagUnder = false;

			}
		}
		else {
			//スライダーでスライドバー以外の移動速度
			if (sliderInfo[uiName].flagTop || sliderInfo[uiName].flagUnder) {
				return;
			}
			float moveY = mY * sliderInfo[uiName].scrollRatioMovPixels;
			y -= moveY;
			addTY = -moveY;
		}
		//ThrowMessageWstr(L"\nscrollRatio : " + to_wstring(sliderInfo[uiName].scrollRatio));

		UpDateMoveTextPos(keyUiParts, idx, addTX, addTY);
		UpDateMoveInsPos(keyUiParts, pos, instance, x, y, z, mX, mY, idx, idxUiStrPs);
	}


}

void UI::UpDateMoveInsPos(const std::wstring& keyUiParts,OriginalInstancePos& pos,InstanceUi &instance,float x,float y,float z, float mX, float mY,int& idx,int& idxUiStrPs)
{
	pos.worldPos.x = x;
	pos.worldPos.y = y;
	pos.worldPos.z = z;

	pos.movX = mX;
	pos.movY = mY;


	uiInfo[keyUiParts].x = x;
	uiInfo[keyUiParts].y = y;
	uiInfo[keyUiParts].z = z;

	//これで、UIの左上を中心としている
	SetUiCenter(x, y);
	instance.world = XMFLOAT4(x, y, z, 0.0f);

	instanceInfo[keyUiParts][idx].world = instance.world;

	idx++;
	idxUiStrPs++;

}

void UI::UpDateMoveTextPos(const std::wstring& keyUiParts, int idxUi, float addTX, float addTY)
{
	auto& texts = textInfo[keyUiParts];
	auto& startTexts = uiStartPos[keyUiParts].uiTextStartPos;

	size_t count = (std::min)(texts.size(), startTexts.size());
	for (size_t textIdx = 0; textIdx < count; ++textIdx) {
		if (texts[textIdx].idxUi != idxUi) {
			continue;
		}

		texts[textIdx].tX = startTexts[textIdx].x + addTX;
		texts[textIdx].tY = startTexts[textIdx].y + addTY;
	}
}

void UI::UpDateMoveTextPos(const std::wstring& uiName, const std::wstring& keyUiParts, float tX, float tY,float tZ , int idx, bool flagUpDateTextPos)
{
	if (!flagUpDateTextPos) return;

	textInfo[keyUiParts][idx].tX = tX;
	textInfo[keyUiParts][idx].tY = tY;

}

void UI::UpDateMoveOnlyTextPos(const std::wstring& uiName, const std::wstring& keyUiParts, float tX, float tY, float tZ, int idx, bool flagUpDateTextPos)
{
	if (!flagUpDateTextPos) return;
	Vec2 startUiPos = uiEventMgr->Getter_uiMoveStartPos();

	int idxText = 0;
	for (auto& text : textInfo[keyUiParts]) {
		float startPosX = uiStartPos[keyUiParts].uiTextStartPos[idxText].x;
		float startPosY = uiStartPos[keyUiParts].uiTextStartPos[idxText].y;

		text.tX = startPosX - tX;
		text.tY = startPosY - tY;
		idxText++;
	}

}


void UI::MoveUi_UI_NODE_NONE_CONF(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node, float mX, float mY)
{
	wstring keyUiParts = registerUiKeyName[uiName][node->nodeName].uiOriginalName;
	InstanceUi instance;

	float x = 0;
	float y = 0;
	float z = 0;

	int idx = 0;

	Vec2 startUiPos = uiEventMgr->Getter_uiMoveStartPos();

	int idxUiStrPs = 0;
	for (auto& pos : originalInstancePos[keyUiParts]) {
		x = uiStartPos[keyUiParts].uiStartPos[idxUiStrPs].x+mX;
		y = uiStartPos[keyUiParts].uiStartPos[idxUiStrPs].y+mY;
		z = pos.worldPos.z;


		pos.worldPos.x = x;
		pos.worldPos.y = y;
		pos.worldPos.z = z;


		SetScissorRect(keyUiParts, true, pos.scissorRectsPosX, pos.scissorRectsPosY, pos.scissorRectsPosX + pos.scissorRectsSizeWidth, pos.scissorRectsPosY + pos.scissorRectsSizeHeight);

		uiInfo[keyUiParts].x = x;
		uiInfo[keyUiParts].y = y;
		uiInfo[keyUiParts].z = z;

		//これで、UIの左上を中心としている
		SetUiCenter(x, y);
		instance.world = XMFLOAT4(x, y, z, 0.0f);

		instanceInfo[keyUiParts][idx].world = instance.world;

		idx++;
		idxUiStrPs++;
	}


}



void UI::UpDateUi(const std::wstring& keyUiName, const vector<InstanceUi>& instanceData)
{
	//この関数でインスタンス描画データをGPU(シェーダー)に送る
	instanceBufferManager->UpDateInstance(keyUiName, instanceData.data(), instanceData.size());
}

void UI::BuildInstanceData(const std::wstring& keyUiName)
{

	const auto& ins = GetInstanceInfo()[keyUiName];
	int idxOrg = 0;
	vector<InstanceUi> insTmp;

	for (const auto& insOrg : GetOriginalInstancePos().at(keyUiName)) {
		if (!insOrg.flagDraw) {
			idxOrg++;
			continue;
		}
		insTmp.emplace_back(ins[idxOrg]);
		idxOrg++;
	}

	GetterDrawNum(keyUiName) = insTmp.size();
	UpDateUi(keyUiName, insTmp);
}


void UI::UiNode(DrawUiNodeType drawUiNodeType,const std::wstring& nodeUIName,const std::wstring& nodeName, const std::wstring& parentName)
{
	//nodeがまだ未登録の時の処理
	if (uiNode[drawUiNodeType][nodeUIName].find(nodeName) == uiNode[drawUiNodeType][nodeUIName].end()) {
		uiNode[drawUiNodeType][nodeUIName][nodeName]=(make_shared<UiRelation>(nodeName));
		uiNode[drawUiNodeType][nodeUIName][nodeName]->nodeName = nodeName;
	}

	//
	if (!parentName.empty()) {
		auto parentIt = uiNode[drawUiNodeType][nodeUIName].find(parentName);  //ここで親ノード取得
		if (parentIt != uiNode[drawUiNodeType][nodeUIName].end()) {
			uiNode[drawUiNodeType][nodeUIName][nodeName]->parent = parentIt->second; //親ノードのポインタを取得して、uiNode[nodeName]->parentに代入(shared_ptr)これはスマートポインタ
			parentIt->second->child.emplace_back(uiNode[drawUiNodeType][nodeUIName][nodeName]); //親の子ノードに追加する
		}
	}

}

void UI::DrawTreeUiNode(const std::shared_ptr<UiRelation>& node, int depth)
{
	if (!node) return;
	wstring null(depth * 2, L' ');
	ThrowMessageWstr(L"\n" + null + L"-" + node->nodeName);

	for (const auto& child : node->child) {
		DrawTreeUiNode(child, depth + 1);
	}
}

void UI::ComposeUI(DrawUiNodeType drawUiNodeType,std::wstring uiName, const std::vector<std::wstring>& uiPartsName)
{
	std::wstring parentNodeName = L"NULL";
	wstring oldKeyUiName;
	wstring fixName;
	for (auto uiParts : uiPartsName) {
		oldKeyUiName = uiParts;
		fixName = oldKeyUiName;
		//もしmapのキーを探していき最後までキーが見つからなかった時
		//以下の処理は、registerUiKeyNameに新旧のキーネームを登録して、後から新しいキーネームを使用する時に古いキーネームｗ押しようできるようにしている
		if (registerUiKeyName[uiName].find(oldKeyUiName) == registerUiKeyName[uiName].end()) {
			registerUiKeyName[uiName][oldKeyUiName].keyCount++;
			registerUiKeyName[uiName][fixName].uiOriginalName = oldKeyUiName;
		}
		//ノード構築
		UiNode(drawUiNodeType,uiName,fixName, parentNodeName);
		parentNodeName = fixName;
		uiFixName[uiName].emplace_back(fixName);
	}
	ThrowMessageWstr(L"\nUIの親子関係表示");
	//親子関係の描画
	DrawTreeUiNode(Getter_uiNode(drawUiNodeType,uiName, uiFixName[uiName][0]));



}

int UI::DrawTextOnUi(std::wstring keyUiPartsName, int idx, std::wstring text, std::string textFormatKey, std::string textColorKey, TextMoveDirect textMoveDirect, bool setTexBoxSize, float x, float y, float z,
	float width, float height, float tX, float tY)
{

	const auto& orgInsPos = originalInstancePos[keyUiPartsName][idx];
	TextInfo textInf;
	if (setTexBoxSize) {
		textInf.x = x;
		textInf.y = y;
		textInf.z = z;
		textInf.tX = tX;
		textInf.tY = tY;
		textInf.idxUi = idx;
		textInf.texBoxHeight = height;
		textInf.texBoxWidth = width;
		textInf.text = text;
		textInf.textFormatKey = textFormatKey;
		textInf.textColorKey = textColorKey;
		textInf.textMoveDirect = textMoveDirect;
		textInf.originalPosX = tX;
		textInf.originalPosY = tY;
		textInf.flagDraw = orgInsPos.flagDraw;
	}
	else {
		textInf.x = orgInsPos.worldPos.x;
		textInf.y = orgInsPos.worldPos.y;
		textInf.z = orgInsPos.worldPos.z;
		textInf.tX = tX;
		textInf.tY = tY;
		textInf.idxUi = idx;
		textInf.texBoxHeight = uiInfo[keyUiPartsName].height;
		textInf.texBoxWidth = uiInfo[keyUiPartsName].width;
		textInf.text = text;
		textInf.textFormatKey = textFormatKey;
		textInf.textColorKey = textColorKey;
		textInf.textMoveDirect = textMoveDirect;
		textInf.originalPosX = tX;
		textInf.originalPosY = tY;
		textInf.flagDraw = orgInsPos.flagDraw;

	}



	textInfo[keyUiPartsName].emplace_back(textInf);
	int texIdx = textInfo[keyUiPartsName].size() - 1;

	return texIdx;
}

void UI::ReplaceUIText(std::wstring keyUiPartsName, int idx, int idxText, std::wstring text, std::string textFormatKey, std::string textColorKey, TextMoveDirect textMoveDirect , bool setTexBoxSize, float x, float y, float z,
	float width , float height, float tX, float tY)
{
	const auto& orgInsPos = originalInstancePos[keyUiPartsName][idx];
	if (textInfo[keyUiPartsName].size() - 1 < idxText) {
		printf("関数名：ReplaceUIText　：　指定されたインデックスが範囲外です");
		ThrrowError("関数名：ReplaceUIText　：　指定されたインデックスが範囲外です");
	}

	TextInfo textInf;
	if (setTexBoxSize) {
		textInf.x = x;
		textInf.y = y;
		textInf.z = z;
		textInf.tX = tX;
		textInf.tY = tY;
		textInf.idxUi = idx;
		textInf.texBoxHeight = height;
		textInf.texBoxWidth = width;
		textInf.text = text;
		textInf.textFormatKey = textFormatKey;
		textInf.textColorKey = textColorKey;
		textInf.textMoveDirect = textMoveDirect;

		textInfo[keyUiPartsName][idxText] = textInf;

	}
	else {

		textInfo[keyUiPartsName][idxText].text = text;

	}

}

void UI::CopyUiParts(std::wstring keyUiPartsName, std::wstring keyNewUiPartsName, float pWidth, float pHeight, float pX, float pY, UIDrawLayer drawLayer)
{
	if (keyNewUiPartsName != L"") {
		//新しいUIパーツのヒープのインデックスを登録する
		uiContext_.uiHeapIdx[keyNewUiPartsName] = uiContext_.uiHeapIdx.size();
		uiInfo[keyNewUiPartsName] = uiInfo[keyUiPartsName];
		//UIテクスチャリソース
		uiTex[keyNewUiPartsName] = uiTex[keyUiPartsName];
		//バーテックスバッファコピー
		//頂点バッファで使用
		resUi[keyNewUiPartsName] = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, vertexBufferMgr->VertexBufferSize(), D3D12_RESOURCE_STATE_GENERIC_READ);

		//新しいUIパーツにテクスチャパス適応させる
		uiKey[keyNewUiPartsName].keyUiTexPass = uiKey[keyUiPartsName].keyUiTexPass;

		void* src = nullptr;
		void* dst = nullptr;

		//コピー元のサイズを取得
		UINT size = uiVbv[keyUiPartsName].SizeInBytes;
		createResource->CopyResource(resUi[keyNewUiPartsName], resUi[keyUiPartsName], size);

		uiVbv[keyNewUiPartsName].BufferLocation = resUi[keyNewUiPartsName]->GetGPUVirtualAddress();
		uiVbv[keyNewUiPartsName].SizeInBytes = vertexBufferMgr->VertexBufferSize();
		uiVbv[keyNewUiPartsName].StrideInBytes = uiVbv[keyUiPartsName].StrideInBytes;

		//インデックスバッファコピー
		//インデックスバッファー生成
		auto indexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto indexResDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferMgr->IndexBufferSize());
		//インデックスバッファー生成
		ThrowIfFailed(DeviceFrom(uiContext_.commandContext)->CreateCommittedResource(
			&indexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&indexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(indexRes[keyNewUiPartsName].ReleaseAndGetAddressOf())));

		createResource->CopyResource(indexRes[keyNewUiPartsName], indexRes[keyUiPartsName], vertexBufferMgr->IndexBufferSize());

		//インデックスバッファビューの生成
		uiIbv[keyNewUiPartsName].BufferLocation = indexRes[keyNewUiPartsName]->GetGPUVirtualAddress();
		uiIbv[keyNewUiPartsName].Format = uiIbv[keyUiPartsName].Format;
		uiIbv[keyNewUiPartsName].SizeInBytes = uiIbv[keyUiPartsName].SizeInBytes;

		//描画する座標などの情報を持たせたコンスタントバッファ作成
		cbUi[keyNewUiPartsName] = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeCb, D3D12_RESOURCE_STATE_GENERIC_READ);


		D3D12_CPU_DESCRIPTOR_HANDLE heapAdd = GetHeap()->GetCPUDescriptorHandleForHeapStart();
		auto inc = DeviceFrom(uiContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		heapAdd.ptr += inc * (UI_ALL_NUM_CBV_SRV)*uiContext_.uiHeapIdx[keyNewUiPartsName];
		//インスタンス描画で使用するSRV作成
		//CBV
		createResource->CopyResource(cbUi[keyNewUiPartsName], cbUi[keyUiPartsName], sizeCb);

		createResource->CreateConstantBufferView(cbUi[keyNewUiPartsName], heapAdd, 0, sizeCb);


		//SRV
		heapAdd.ptr += inc;

		//テクスチャロード(SRV)
		createResource->CreateShaderResourceView(uiTex[uiKey[keyNewUiPartsName].keyUiTexPass].Get(), heapAdd);
		uiDrawPos[keyNewUiPartsName].uvPos = ExtractTextureRegion(uiTex[uiKey[keyNewUiPartsName].keyUiTexPass], true, pWidth, pHeight, pX, pY);
		RegisterUIDrawOrder(keyNewUiPartsName, drawLayer);

	}
	else {
		ThrrowError("新しいキー名が指定されていません");

	}
}


void UI::CreateUiPartsButton(std::wstring keyUiPartsName, std::wstring keyNewUiPartsName, float pWidth, float pHeight, float pX, float pY)
{
	if (keyNewUiPartsName != L"") {
		//新しいUIパーツのヒープのインデックスを登録する
		uiContext_.uiHeapIdx[keyNewUiPartsName] = uiContext_.uiHeapIdx.size();
		uiInfo[keyNewUiPartsName] = uiInfo[keyUiPartsName];
		//UIテクスチャリソース
		uiTex[keyNewUiPartsName] = uiTex[keyUiPartsName];
		//バーテックスバッファコピー
		//頂点バッファで使用
		resUi[keyNewUiPartsName] = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, vertexBufferMgr->VertexBufferSize(), D3D12_RESOURCE_STATE_GENERIC_READ);

		//新しいUIパーツにテクスチャパス適応させる
		uiKey[keyNewUiPartsName].keyUiTexPass = uiKey[keyUiPartsName].keyUiTexPass;

		void* src = nullptr;
		void* dst = nullptr;

		//コピー元のサイズを取得
		UINT size = uiVbv[keyUiPartsName].SizeInBytes;
		createResource->CopyResource(resUi[keyNewUiPartsName], resUi[keyUiPartsName], size);

		uiVbv[keyNewUiPartsName].BufferLocation = resUi[keyNewUiPartsName]->GetGPUVirtualAddress();
		uiVbv[keyNewUiPartsName].SizeInBytes = vertexBufferMgr->VertexBufferSize();
		uiVbv[keyNewUiPartsName].StrideInBytes = uiVbv[keyUiPartsName].StrideInBytes;

		//インデックスバッファコピー
		//インデックスバッファー生成
		auto indexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto indexResDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferMgr->IndexBufferSize());
		//インデックスバッファー生成
		ThrowIfFailed(DeviceFrom(uiContext_.commandContext)->CreateCommittedResource(
			&indexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&indexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(indexRes[keyNewUiPartsName].ReleaseAndGetAddressOf())));

		createResource->CopyResource(indexRes[keyNewUiPartsName], indexRes[keyUiPartsName], vertexBufferMgr->IndexBufferSize());

		//インデックスバッファビューの生成
		uiIbv[keyNewUiPartsName].BufferLocation = indexRes[keyNewUiPartsName]->GetGPUVirtualAddress();
		uiIbv[keyNewUiPartsName].Format = uiIbv[keyUiPartsName].Format;
		uiIbv[keyNewUiPartsName].SizeInBytes = uiIbv[keyUiPartsName].SizeInBytes;

		//描画する座標などの情報を持たせたコンスタントバッファ作成
		cbUi[keyNewUiPartsName] = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeCb, D3D12_RESOURCE_STATE_GENERIC_READ);


		D3D12_CPU_DESCRIPTOR_HANDLE heapAdd = GetHeap()->GetCPUDescriptorHandleForHeapStart();
		auto inc = DeviceFrom(uiContext_.commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		heapAdd.ptr += inc * (UI_ALL_NUM_CBV_SRV)*uiContext_.uiHeapIdx[keyNewUiPartsName];
		//インスタンス描画で使用するSRV作成
		//CBV
		createResource->CopyResource(cbUi[keyNewUiPartsName], cbUi[keyUiPartsName], sizeCb);

		createResource->CreateConstantBufferView(cbUi[keyNewUiPartsName], heapAdd, 0, sizeCb);


		//SRV
		heapAdd.ptr += inc;

		//テクスチャロード(SRV)
		createResource->CreateShaderResourceView(uiTex[uiKey[keyNewUiPartsName].keyUiTexPass].Get(), heapAdd);
		uiDrawPos[keyNewUiPartsName].uvPos = uiDrawPos[keyUiPartsName].uvPos;
		//UV座標の設定
		changeUv[keyNewUiPartsName].uv = uiDrawPos[keyUiPartsName].uvPos;
		changeUv[keyNewUiPartsName].uvCh = ExtractTextureRegion(uiTex[uiKey[keyNewUiPartsName].keyUiTexPass], true, pWidth, pHeight, pX, pY);
	}
	else {
		//新しいキー名無ければ上書きする
		changeUv[keyUiPartsName].uv = uiDrawPos[keyUiPartsName].uvPos;
		changeUv[keyUiPartsName].uvCh = ExtractTextureRegion(uiTex[keyUiPartsName], true, pWidth, pHeight, pX, pY);

	}
}

void UI::CreateUiPartsSlider(DrawUiNodeType drawUiNodeType, std::wstring keyUiName, const std::vector<std::wstring>& uiPartsName, float x, float y, float scissorRectsSizeWidth, float scissorRectsSizeHeight, int totalTextSize)
{
	if (drawUiNodeType != UI_NODE_SLIDER_HORIZON && drawUiNodeType != UI_NODE_SLIDER_VERTICAL && drawUiNodeType != UI_NODE_SLIDER_VERTICAL_ONLY_TEXT) {
		ThrowMessageWstr(L"\nこの関数はUIスライダーのノード作成関数です。\ndrawUiNodeTypeが適切ではありません。");
		return;
	}
	uiNodeType[keyUiName] = drawUiNodeType;
	Slider slider;
	//orgInsPos=スクロールバーの背景
	OriginalInstancePos* orgInsPosScrollBackGround = nullptr;
	OriginalInstancePos* orgInsPosScrollBar = nullptr;


	float max = 0;
	float min = 0;
	float uiHeight = 0;
	bool flagFirst = true;



	switch (drawUiNodeType) {
	case UI_NODE_SLIDER_HORIZON:
	{
		//UI_SLIDER_HORIZON_SCROLLBAR_BACKGROUNDがあるかどうか探す
		for (auto uiParts : uiPartsName) {
			orgInsPosScrollBackGround = FindSliderRole(uiParts, SLIDER_HORIZON_SCROLLBAR_BACKGROUND);
			if (orgInsPosScrollBackGround != nullptr) {
				break;
			}
		}

		if (orgInsPosScrollBackGround == nullptr) {
			break;
		}


		//UIノード組み立て
		ComposeUI(drawUiNodeType, keyUiName, uiPartsName);
		for (const auto& name : uiPartsName) {
			for (const auto& size : originalInstancePos[name]) {
				if (size.uiPartMeta.sliderRole != SLIDER_HORIZON) {
					continue;
				}
				slider.totalSize += size.uiWidth;
			}
		}

		break;
	}
	case UI_NODE_SLIDER_VERTICAL:
	{
		//UI_SLIDER_VERTICAL_SCROLLBAR_BACKGROUNDがあるかどうか探す
		for (auto uiParts : uiPartsName) {
			orgInsPosScrollBackGround = FindSliderRole(uiParts, SLIDER_VERTICAL_SCROLLBAR_BACKGROUND);
			if (orgInsPosScrollBackGround != nullptr) {
				slider.x = orgInsPosScrollBackGround->worldPos.x;
				slider.y = orgInsPosScrollBackGround->worldPos.y;
				break;
			}
		}

		if (orgInsPosScrollBackGround == nullptr) {
			break;
		}
		//シザーレクトセット
		slider.scissorRectsSizeHeight = scissorRectsSizeHeight;
		slider.scissorRectsSizeWidth = scissorRectsSizeWidth;


		ComposeUI(drawUiNodeType, keyUiName, uiPartsName);

		//UIスライダーノードのみのループ
		for (const auto& name : uiPartsName) {
			SetScissorRect(name, true, x, y,
				x + slider.scissorRectsSizeWidth, y + slider.scissorRectsSizeHeight);

			for (auto& size : originalInstancePos[name]) {
				size.scissorRectsSizeHeight = slider.scissorRectsSizeHeight;
				size.scissorRectsSizeWidth = slider.scissorRectsSizeWidth;
				size.scissorRectsPosX = x;
				size.scissorRectsPosY = y;

				if (size.uiPartMeta.sliderRole != SLIDER_VERTICAL) {
					continue;
				}
				//最大値最小値を求める
				if (max < size.worldPos.y) {
					max = size.worldPos.y;
				}
				if (min > size.worldPos.y || flagFirst) {
					min = size.worldPos.y;
					flagFirst = false;
				}

				uiHeight = size.uiHeight;
			}


			for (auto& textInf : textInfo[name]) {
				//初めにセットしたクリップをベースとしたテキストの座標を維持したまま移動するために差分の移動を求める
				float diffX = textInf.x + textInf.tX - x;
				float diffY = textInf.y + textInf.tY - y;

				textInf.x = x;
				textInf.y = y;
				textInf.tX = diffX;
				textInf.tY = diffY;
				textInf.texBoxHeight = scissorRectsSizeHeight;
				textInf.texBoxWidth = scissorRectsSizeWidth;
			}

		}



		slider.totalSize = max - min + uiHeight;



		break;

	}
	case UI_NODE_SLIDER_VERTICAL_ONLY_TEXT:
	{
		//UI_SLIDER_VERTICAL_SCROLLBAR_BACKGROUNDがあるかどうか探す
		for (auto uiParts : uiPartsName) {
			orgInsPosScrollBackGround = FindSliderRole(uiParts, SLIDER_VERTICAL_SCROLLBAR_BACKGROUND);
			if (orgInsPosScrollBackGround != nullptr) {
				slider.x = orgInsPosScrollBackGround->worldPos.x;
				slider.y = orgInsPosScrollBackGround->worldPos.y;
				break;
			}
		}

		if (orgInsPosScrollBackGround == nullptr) {
			break;
		}
		//シザーレクトセット
		slider.scissorRectsSizeHeight = orgInsPosScrollBackGround->uiHeight;
		slider.scissorRectsSizeWidth = scissorRectsSizeWidth;

		ComposeUI(drawUiNodeType, keyUiName, uiPartsName);

		//UIスライダーノードのみのループ


		slider.totalSize = totalTextSize;

		break;
	}
	default:
		ThrowMessageWstr(L"\ndrawUiNodeTypeを変更してください");
	}



	float ratio = slider.scissorRectsSizeHeight / (std::max)(slider.totalSize, 1.0f);
	ratio = std::clamp(ratio, 0.0f, 1.0f);

	wstring keyUiPartsName;
	int idx = 0;
	for (auto uiParts : uiPartsName) {
		orgInsPosScrollBar = FindSliderRole(uiParts, SLIDER_VERTICAL_SCROLLBAR, &idx);
		if (orgInsPosScrollBar != nullptr) {
			keyUiPartsName = uiParts;
			break;
		}
	}

	if (orgInsPosScrollBar == nullptr) {
		ThrowMessageWstr(L"\nUI_SLIDER_VERTICAL_SCROLLBARが見つかりませんでした");
		return;
	}

	if (orgInsPosScrollBackGround == nullptr) {
		return;
	}


	//スクロール範囲の上限と下限設定
	slider.scrollMin = orgInsPosScrollBackGround->worldPos.y;
	slider.scrollMax = orgInsPosScrollBackGround->worldPos.y + orgInsPosScrollBackGround->uiHeight;

	//UIスライダーの要素数に合わせてサイズを変化させた後の値
	float sliderBarHeight = orgInsPosScrollBar->uiHeight * ratio;
	uiInfo[keyUiPartsName].width = orgInsPosScrollBar->uiWidth;
	uiInfo[keyUiPartsName].height = sliderBarHeight;
	uiInfo[keyUiPartsName].x = orgInsPosScrollBar->worldPos.x;
	uiInfo[keyUiPartsName].y = orgInsPosScrollBar->worldPos.y;
	uiInfo[keyUiPartsName].z = orgInsPosScrollBar->worldPos.z;

	slider.scrollBarOrgInsPos = orgInsPosScrollBar;

	slider.scrollbarSizeHeight = sliderBarHeight;
	slider.scrollbarSizeWidth = uiInfo[keyUiPartsName].width;
	slider.scrollRatioMovPixels = slider.totalSize / slider.scissorRectsSizeHeight;

	slider.scrollRatio = ratio;


	sliderInfo[keyUiName] = slider;

	//スクロールバーの大きさをUIのスライダーの要素の大きさに合わせて動的に変化させる
	float height = sliderBarHeight;
	float width = uiInfo[keyUiPartsName].width;


	float iX = orgInsPosScrollBackGround->worldPos.x;
	float iY = orgInsPosScrollBackGround->worldPos.y;
	SetUiCenter(iX, iY);

	//インスタンス描画用の座標をスクロールバーのバックグラウンドの描画位置に合わせている
	orgInsPosScrollBar->worldPos.x = orgInsPosScrollBackGround->worldPos.x;
	orgInsPosScrollBar->worldPos.y = orgInsPosScrollBackGround->worldPos.y;
	//orgInsPosScrollBar->uiHeight = uiInfo[keyUiPartsName].height;
	orgInsPosScrollBar->uiWidth = uiInfo[keyUiPartsName].width;

	instanceInfo[keyUiPartsName][idx].world = XMFLOAT4(iX, iY, orgInsPosScrollBackGround->worldPos.z, 0.0f);


	vertexBufferMgr->CreateVertexBuffer(resUi[keyUiPartsName], indexRes[keyUiPartsName], uiVbv[keyUiPartsName], uiIbv[keyUiPartsName], VertexBufferType::UI, width, height, uiInfo[keyUiPartsName].z);
}

OriginalInstancePos* UI::FindSliderRole(std::wstring keyUiPartsName, SliderPartsRole sliderPartsRole, int* idxIns)
{
	int idx = 0;
	for (const auto& orgIns : originalInstancePos[keyUiPartsName]) {
		if (orgIns.uiPartMeta.sliderRole == sliderPartsRole) {
			//idxInsがnullptrでなければtrue返却
			if (idxIns) {
				*idxIns = idx;
			}
			return &originalInstancePos[keyUiPartsName][idx];
		}
		idx++;
	}

	return nullptr;
}





OriginalInstancePos* UI::FindUiDrawType(std::wstring keyUiPartsName,DrawUiPartsType drawUiPartsType, int* idxIns)
{
	int idx = 0;
	for (const auto& orgIns : originalInstancePos[keyUiPartsName]) {
		if (orgIns.uiPartMeta.drawType == drawUiPartsType) {
			//idxInsがnullptrでなければtrue返却
			if (idxIns) {
				*idxIns = idx;
			}
			return &originalInstancePos[keyUiPartsName][idx];
		}
		idx++;
	}

	return nullptr;
}

void UI::SetScissorRect(std::wstring keyUiPartsName,bool flagSetScissor , float x , float y , float width , float height)
{
	if (!flagSetScissor) {
		CD3DX12_RECT scissorrectUI_(0, 0, win.width, win.height);
		scissorRect[keyUiPartsName] = scissorrectUI_;
	}
	else {
		CD3DX12_RECT scissorrectUI_(x, y, width, height);
		scissorRect[keyUiPartsName] = scissorrectUI_;
	}
}

void UI::DeleteInsAndOrgIns(const std::wstring& keyUiPartsName, UINT idx)
{
	auto& uiIns = GetInstanceInfo()[keyUiPartsName];
	auto& orgIns = GetOriginalInstancePos()[keyUiPartsName];

	if (uiIns.size() == 0 || orgIns.size() == 0) {
		return;
	}

	uiIns.erase(uiIns.begin() + idx);
	orgIns.erase(orgIns.begin() + idx);

}

void UI::FindUIElementAtCursor(const std::wstring& keyUiPartsName,const Vec2& mPos)
{
	GetUiHoverHitResult();
	if (GetUiHoverHitResult().isHit) {
		return;
	}

	int idx = 0;
	//UIの上にマウスカーソルが来た時に、どのUIなのかの情報を確保する
	for (const auto& orgIns : GetOriginalInstancePos()[keyUiPartsName]) {
		if (orgIns.worldPos.x <= mPos.X && orgIns.worldPos.x + orgIns.uiWidth >= mPos.X
			&& orgIns.worldPos.y <= mPos.Y && orgIns.worldPos.y + orgIns.uiHeight >= mPos.Y) {
			GetUiHoverHitResult().isHit = true;
			GetUiHoverHitResult().index = idx;
			GetUiHoverHitResult().drawUiPartsName = keyUiPartsName;
		}
		idx++;
	}


}

void UI::SetTextPosition(const std::wstring& keyUiPartsName, int textIdx, float tX, float tY)
{
	auto it = textInfo.find(keyUiPartsName);
	if(it == textInfo.end()) {
		return;
	}

	if (textIdx < 0 || textIdx >= it->second.size()) {
		return;
	}

	auto& info = it->second[textIdx];

	info.tX = tX;
	info.tY = tY;
}

//UIイベント処理/////////////////////////////////////

bool UI::FindClickUIFromText(const std::wstring& keyUiPartsName,const std::wstring& uiText,MouseButton mouseButton) {
	int idx = GetUiPartsIdx(keyUiPartsName, mouseButton);


	if (idx >= 0) {
		bool flagUIScreen = false;

		for (const auto& text : GetTextInfo().at(keyUiPartsName)) {
			if (text.idxUi == idx && text.text == uiText) {
				return true;
				break;
			}
		}

	}

	return false;
}


void UI::HighlightUI(const std::wstring& keyBaceUiPartsName, const std::wstring& keyHighLightUIPartsName ,DrawUiPartsType drawUiPartsType, const Vec2& mPos)
{
	int idx = 0;
	if (!FindKey(keyBaceUiPartsName, uiInfo)) {
		return;
	}

	if (!FindKey(keyHighLightUIPartsName, uiInfo)) {
		return;
	}

	if (!FindKey(keyBaceUiPartsName, originalInstancePos)) {
		return;
	}

	const auto& uiInfBace = uiInfo[keyBaceUiPartsName];
	auto& uiInfHighLight = originalInstancePos[keyHighLightUIPartsName];

	

	for (auto& hl : uiInfHighLight) {
		hl.flagDraw = false;
	}


	for (const auto& orgInsPos : originalInstancePos[keyBaceUiPartsName]) {
		if (orgInsPos.uiPartMeta.drawType != drawUiPartsType || orgInsPos.flagDraw == false) {
			continue;
		}
		//マウスのをUI上に持ってきた瞬間
		if ((orgInsPos.worldPos.x <= mPos.X && orgInsPos.worldPos.x + uiInfBace.width >= mPos.X) &&
			(orgInsPos.worldPos.y <= mPos.Y && orgInsPos.worldPos.y + uiInfBace.height >= mPos.Y)) {
			//ThrowMessageWstr(L"\nボタンクリック!!");
			//PostDrawUi(keyHighLightUIPartsName, UI_NONE_CONF, orgInsPos.worldPos.x, orgInsPos.worldPos.y, orgInsPos.worldPos.z);
			uiInfHighLight[idx].flagDraw = true;
			return;
			break;
		}
		else {
			auto& uiInsHighLight = instanceInfo[keyHighLightUIPartsName];
			if (uiInsHighLight.size() > 0 && uiInsHighLight.size() > idx && idx>=0) {
				//マウスがUI上に無ければインスタンスから、要素削除
				//uiInsHighLight.erase(uiInsHighLight.begin() + idx);
				uiInfHighLight[idx].flagDraw = false;
			}
		}
		idx++;
	}


}

int UI::GetUiPartsIdx(std::wstring keyUiPartsName, MouseButton mouseButton)
{
	auto scissorRect = Getter_scissorRect(keyUiPartsName);

	//ボタンをクリックしたUIのインデックスを返却する
	int orgInsPosIdx = uiEventMgr->ClickUi(mouseButton, UI_BUTTON, originalInstancePos[keyUiPartsName], uiInfo[keyUiPartsName], scissorRect);
	if (orgInsPosIdx == -1) {
		return orgInsPosIdx;
	}
	return orgInsPosIdx;
}



void UI::UiPartsPushButton(std::wstring keyUiPartsName,MouseButton mouseButton)
{
	auto scissorRect = Getter_scissorRect(keyUiPartsName);
	
	//ボタンをクリックしたUIのインデックスを返却する
	int orgInsPosIdx=uiEventMgr->ClickUi(mouseButton, UI_BUTTON,originalInstancePos[keyUiPartsName], uiInfo[keyUiPartsName], scissorRect);
	if (orgInsPosIdx == -1) {
		return;
	}

	if (changeUv.find(keyUiPartsName) != changeUv.end()) {
		//もしキーが存在しているなら
		if (!changeUv[keyUiPartsName].flagClick) {
			//クリックしていなければクリック時のUV座標へ
			originalInstancePos[keyUiPartsName][orgInsPosIdx].uv = changeUv[keyUiPartsName].uvCh;
			instanceInfo[keyUiPartsName][orgInsPosIdx].uv = originalInstancePos[keyUiPartsName][orgInsPosIdx].uv;
			changeUv[keyUiPartsName].flagClick = true;
		}
		else if (changeUv[keyUiPartsName].flagClick) {
			//クリックしていればクリック前のUV座標へ
			originalInstancePos[keyUiPartsName][orgInsPosIdx].uv = changeUv[keyUiPartsName].uv;
			instanceInfo[keyUiPartsName][orgInsPosIdx].uv = originalInstancePos[keyUiPartsName][orgInsPosIdx].uv;
			changeUv[keyUiPartsName].flagClick = false;
		}
	}
}

void UI::MoveUi(DrawUiNodeType drawUiNodeType, const std::wstring& uiName, const std::wstring& drawKeyUiPartsName, const std::wstring& keyUiPartsName, MouseButton mouseButton, UiPartMeta uiPartsMeta, float x, float y)
{
	if (drawKeyUiPartsName != keyUiPartsName) {
		return;
	}
	const std::shared_ptr<UiRelation>& node = Getter_uiNode(drawUiNodeType,uiName, drawKeyUiPartsName);
	//nodeが空であれば関数の処理終了
	if (!node) {
		return;
	}

	if (keyUiPartsName != node->nodeName) {
		return;
	}

	Vec2& mStartPos = uiEventMgr->ClickStartPos();

	//ドラッグ中でなければ当たり判定
	if (!isDraggingMoveUI[uiName]) {
		int orgInsPosIdx = uiEventMgr->PushUi(
			mouseButton, uiPartsMeta,
			originalInstancePos[keyUiPartsName],
			uiInfo[keyUiPartsName], mStartPos);
		if (orgInsPosIdx == -1) {
			EraseUiStartPosRecursive(uiName, node);
			return;
		}
		isDraggingMoveUI[uiName] = true; //ここで初めてセット
	}

	//マウスボタンを離したら終了
	if (uiService_.mouseInput.OnMouseUp(mouseButton)) {
		isDraggingMoveUI[uiName] = false;
		//ドラッグ終了したら、UIの開始位置の情報を削除する
		//ノードを辿って、子ノードのUIパーツの開始位置の情報も削除する
		//自分自身も消す
		EraseUiStartPosRecursive(uiName, node);

		sliderInfo[uiName].flagTop = false;
		sliderInfo[uiName].flagUnder = false;
		return;
	}

	Vec2 nowMousePos = uiService_.mouseInput.GetMousePos();
	//printf("\nマウスnowMousePos Y : %f", nowMousePos.Y);

	Vec2 deltaPos = nowMousePos - mStartPos;

	ThrowMessageWstr(L"\nX:" + to_wstring(deltaPos.X) + L"\nY:" + to_wstring(deltaPos.Y));

	MoveDrawPoint(uiName,node, deltaPos.X, deltaPos.Y);
}

void UI::MoveUiToPosition(DrawUiNodeType drawUiNodeType, const std::wstring& uiName, const std::wstring& drawKeyUiPartsName, float x, float y)
{
	const std::shared_ptr<UiRelation>& node = Getter_uiNode(drawUiNodeType, uiName, drawKeyUiPartsName);
	//nodeが空であれば関数の処理終了
	if (!node) {
		return;
	}

	//根元のキーを取得
	const std::wstring rootKey = registerUiKeyName[uiName][node->nodeName].uiOriginalName;

	auto rootIt = originalInstancePos.find(rootKey);

	if (rootIt == originalInstancePos.end() ||
		rootIt->second.empty()) {
		return;
	}

	const auto& rootPosition = rootIt->second.front().worldPos;

	//ルートUIを目的座標へ移動させるための差分
	const float moveX = x - rootPosition.x;
	const float moveY = y - rootPosition.y;

	NodeSetDrawPosition(uiName, node, moveX, moveY);
}

void UI::EraseUiStartPosRecursive(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node)
{
	if (!node) {
		return;
	}

	std::wstring keyUiParts = registerUiKeyName[uiName][node->nodeName].uiOriginalName;
	uiStartPos.erase(keyUiParts);

	for (const auto& child : node->child) {
		EraseUiStartPosRecursive(uiName, child);
	}
}

bool UI::SetDrawPosition(const std::wstring& keyUiPartsName,std::size_t index,float x,float y,float z) 
{
	//UIキーが実在するものかを調べる
	auto orgIt = originalInstancePos.find(keyUiPartsName);
	auto insIt = instanceInfo.find(keyUiPartsName);

	if (orgIt == originalInstancePos.end() || insIt == instanceInfo.end()) {
		return false;
	}

	auto& originalPositions = orgIt->second;
	auto& instances = insIt->second;

	//指定されたインデックスがアクセス範囲外かどうかを確認する
	if (index >= originalPositions.size() || index >= instances.size()) {
		return false;
	}

	auto& original = originalPositions[index];

	//移動量を求める
	const float moveX = x - original.worldPos.x;
	const float moveY = y - original.worldPos.y;

	original.worldPos = XMFLOAT4(x, y, z, 0.0f);

	float ndcX = x;
	float ndcY = y;
	SetUiCenter(ndcX, ndcY);

	instances[index].world = XMFLOAT4(ndcX, ndcY, z, 0.0f);

	uiInfo[keyUiPartsName].x = x;
	uiInfo[keyUiPartsName].y = y;
	uiInfo[keyUiPartsName].z = z;

	//UIに対応するテキストも移動
	for(auto& text : textInfo[keyUiPartsName]) {
		if (text.idxUi != static_cast<int>(index)) {
			continue;
		}

		text.x += moveX;
		text.y += moveY;
	}

	return true;
}

void UI::NodeSetDrawPosition(const std::wstring& uiNodeName, const std::shared_ptr<UiRelation>& node, float x, float y)
{
	if (!node) {
		flagBackGround = false;
		return ;
	}

	wstring keyUiParts = registerUiKeyName[uiNodeName][node->nodeName].uiOriginalName;

	int idx = 0;
	for (const auto& orgPos : originalInstancePos[keyUiParts]) {
		SetDrawPosition(keyUiParts, idx, orgPos.worldPos.x + x, orgPos.worldPos.y + y, 0);
		idx++;
	}


	for (auto uinode : node->child) {
		NodeSetDrawPosition(uiNodeName, uinode, x, y);
	}

	return ;
}



///////////////////////////////////////////////////////