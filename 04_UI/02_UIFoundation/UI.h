#pragma once

#include<d3d12.h>
#include"d3dx12.h"
#include<DirectXMath.h>
#include<unordered_map>
#include<string>
#include <memory>
#include<chrono>
#include<array>
#include<optional>

#pragma comment(lib,"libfbxsdk-md.lib")
#pragma comment(lib,"libxml2-md.lib")
#pragma comment(lib,"zlib-md.lib")
#pragma comment(lib, "d3d12.lib")

#include"helper.h"
#include"GraphicsUtility.h"
#include"DXAppUtillity.h"
#include"MouseInput.h"
#include"IUIScreen.h"
#include"Uiutillity.h"
#include"Engine/DrawText/ITextRenderer.h"


class CreateResource;
class LoadTex;
class DirectWrite_2D;
class MouseInput;
class UiEventManager;
class VertexBufferManager;
class InstanceBufferManager;

namespace Engine
{
	class ITextRenderer;
}

//UIクラスの使用
// 1 :クラスを使えるようにする
// 2 :CreateUiでテクスチャや初期のUIサイズなど設定
// ExtractUITexこれで、一枚のUIテクスチャから切り抜いて新しいUIパーツとして使用できるようにする
// CreateUiPartsButtonこれで、UIのボタンなどクリックしたときのUV座標を変更するための座標を登録
// 3 : DrawUiでUIのパーツ名をキーとして実行する。実行した数だけそのキーのパーツを描画する
// 4 : ComposeUIこれで、UIの親子関係を構築してUIを構築する
// 5 : UpDateUiこれでUIのインスタンス描画で使用するデータをシェーダーに送信
// UiAnimこれは、UIのアニメーションがあれば、これで更新

struct UIService
{
	Engine::ITextRenderer& textRenderer;
	MouseInput& mouseInput;

	UIService(Engine::ITextRenderer& textRenderer_, MouseInput& mi)
		: textRenderer(textRenderer_)
		, mouseInput(mi)
	{
	}

};

struct UIContext
{
	CommandContext& commandContext;
	ComPtr<ID3D12DescriptorHeap> heap;
	std::unordered_map<std::wstring, int>& uiHeapIdx;
	ViewportScissor viewportScissor;
	UIPipeline uiPipe;

	UIContext(
		CommandContext& commandContext_,
		ComPtr<ID3D12DescriptorHeap> heap_,
		std::unordered_map<std::wstring, int>& uiHeapIdx_,
		const ViewportScissor& viewportScissor_,
		const UIPipeline& uiPipe_
	)
		: commandContext(commandContext_)
		, heap(heap_)
		, uiHeapIdx(uiHeapIdx_)
		, viewportScissor(viewportScissor_)
		, uiPipe(uiPipe_)
	{
	}
};

//UIパーツごとの設定情報(UIごとのサイズのみを取得)
struct UIPartLayout
{
	float width;
	float height;
	float x;
	float y;
};


class UI :public IUIScreen
{
public:
	UI(UIService uiService,
		UIContext uiContext);
	~UI();
	std::shared_ptr<CreateResource> createResource;
	std::shared_ptr<LoadTex> loadTex;
	std::shared_ptr<UiEventManager> uiEventMgr;

	//UIの共通更新処理（特別な処理用意する必要ないとき）
	void UpdateCommon();
	
	//UIの共通描画処理（特別な処理用意する必要ないとき）
	void DrawCommon();

	// テキストの描画位置を変更する
	void SetTextPosition(const std::wstring& keyUiPartsName, int textIdx, float tX, float tY);

	//UI用のバーテックスバッファとUIのテクスチャリソースビュー作成（座標は、スクリーンスペース座標）
	//keyUiPartsName : UI部品の名前
	//keyUiTexName : UIテクスチャのパス
	//flagDrawTexSize : テクスチャのサイズで描画(true : テクスチャのサイズで描画 / false : 指定サイズで描画)
	//flagAnim : UIにアニメーションさせるか
	//x : 描画座標＿X
	//y : 描画座標＿Y
	//z  : 画像の表示するZ座標
	//width : UIの幅
	//height : UIの高さ
	//rote   : 回転
	//drawLayer : UIの描画順のレイヤー
	void CreateUi(std::wstring keyUiPartsName, std::wstring keyUiTexPass, bool flagDrawTexSize = false, bool flagAnim = false,float x = 0.0f, float y = 0.0f, float z = 0.0f ,float width = 0.0f, float height = 0.0f,
		float rote = 0.0f, UIDrawLayer drawLayer = UIDrawLayer::Background);

	//ExtractUITexでどのようにUIのサイズを設定するか

	//UV座標を指定してテクスチャを切り抜く
	// keyUiPartsName : 使用するUIのテクスチャ
	// keyNewUiPartsName : 切り抜いて使用するUIパーツの名前
	// flagNoClip : 切り抜きをするのかそれともそのまま描画するのか（true = 切り抜く / false = 切り抜かない）
	// drawSizeType : リソース生成時のUIサイズをどのサイズで描画するか（DRAW_TEX_SIZE = テクスチャのサイズで描画 ,DRAW_ORIGINAL_SIZE = CreateUi時のサイズで描画 ,DRAW_SET_SIZE = UIのサイズを新しくセットする）
	// width : 切り抜く画像の横幅
	// height: 切り抜く画像の縦幅
	// x     : テクスチャの切り抜きを始めるx座標
	// y	 : テクスチャの切り抜きを始めるy座標
	// setSizeWidth : drawSizeTypeでDRAW_SET_SIZEを選択したときのUIの横幅
	// setSizeHeight : drawSizeTypeでDRAW_SET_SIZEを選択したときのUIの縦幅
	// drawLayer : UIの描画順のレイヤー
	void ExtractUITex(std::wstring keyUiPartsName, std::wstring keyNewUiPartsName, bool flagNoClip=false, DrawSizeType drawSizeType = DRAW_ORIGINAL_SIZE,
		float width=0.0f, float height = 0.0f, float x = 0.0f, float y = 0.0f, float setSizeWidth = 0.0f, float setSizeHeight=0.0f ,UIDrawLayer drawLayer = UIDrawLayer::Background);


	//UIのアニメーション
	// keyUiPartsName : UI部品の名前キー
	// idx : originalInstancePosのインデックス
	// flagChangeUiSize : UIを切り抜くサイズで描画するかどうか（true : 切り抜くサイズで描画 / false : 基の設定したサイズで描画）
	// width : 切り抜く画像の横幅
	// height: 切り抜く画像の縦幅
	// x     : テクスチャの切り抜きを始めるx座標
	// y	 : テクスチャの切り抜きを始めるy座標
	// changeTime : アニメーションの切り替え時間
	void UiAnim(std::wstring keyUiPartsName, int idx, bool flagChangeUiSize = false,float width = 0.0f, float height = 0.0f, float x = 0.0f, float y = 0.0f, float changeTime = 1.0f);
	//フォントサイズを返却する
	float GetFontSize(const std::string& textFont) const;

	//UIの高さのセンターに文字が来るようにオフセットを返却する
	//fontKey : 対象フォントキー
	//uiSize : 対象UIのサイズ
	float CalculateCenteredTextYOffset(const std::string& fontKey, float uiSize);

	//UIの横幅のセンターに文字が来るようにオフセットを返却する
	//fontKey : 対象フォントキー
	//uiSize : 対象UIのサイズ
	float CalculateCenteredTextXOffset(const std::string& fontKey, const std::wstring& text, float uiSize);


	//テキストの横の長さオフセットを計算する
	float CalculateTextHorizontalOffset(int textNum, float textSize) const;

	//UI親子関係構築
	struct UiRelation
	{
		std::shared_ptr<UiRelation> parent; //親キー
		std::vector<std::shared_ptr<UiRelation>> child; //子キー
		std::wstring nodeName; //ノードネーム

		UiRelation(const std::wstring& nodeName) : nodeName(nodeName), parent(nullptr)
		{

		}

	};

	//インスタンス描画用の構造体
	struct alignas(16) InstanceUi
	{
		DirectX::XMFLOAT4 world; //UIの位置座標
		DirectX::XMFLOAT4 uv;	//UIのUV値
		DirectX::XMFLOAT4X4 rote;	//UIの回転量
		InstanceUi()
		{
			world = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			uv.x = 0; //UV左上
			uv.y = 0; //UV左上
			uv.w = 1; //UV右下
			uv.z = 1; //UV右下

		}
	};



	//描画位置をずらす
	// uiName : UIの塊（親子関係のノード名）
	// node : 動かすUIのノード
	// x : 移動座標＿X
	// y : 移動座標＿Y
	void MoveDrawPoint(const std::wstring& uiName,const std::shared_ptr<UiRelation>& node, float x = 0.0f,float y=0.0f);

	//UI上にテキスト描画
	void DrawUiTex();

	//UIのアップデート処理(インスタンス描画で使用するデータをシェーダーに送信する)
	void UpDateUi(const std::wstring& keyUiName, const std::vector<InstanceUi>& instanceData);

	//UI描画のためのインスタンス描画用リソースビュー生成している (描画したときUIのインデックスを返却)
	// keyUiPartsName : UI部品の名前のキー名
	// drawUiPartsType : 描画するUIパーツのタイプ (UI_NONE_CONF 設定なし, UI_BACKGROUND 背景,UI_BUTTON ボタン,	UI_SLIDER_VERTICAL_BAR スライダー縦のバー,UI_SLIDER_VERTICAL スライダー 縦, UI_SLIDER_HORIZON_BAR スライダー横のバー,UI_SLIDER_HORIZON スライダー横)	
	// x         : 描画座標＿X
	// y		 : 描画座標＿Y
	// z		 : 描画座標＿Z
	// rote		 : 回転量 (度)
	// flagDraw : 描画するかどうかのフラグ
	// flagAnimation : UIアニメーションするかどうか
	// flagWriteText : UI上にテキストを描画するかどうか（true : 描画する,false : 描画しない）
	int PostDrawUi(std::wstring keyUiPartsName, UiPartMeta uiPartMeta = {}, float x = 0.0f, float y = 0.0f, float z = 0.0f, float rote = 0.0f, bool flagDraw = true, bool flagAnimation = false, bool flagWriteText = false);

	//UI用のテクスチャ取得
	//keyUiPartsName : UI部品の名前のキー
	//texPass : テクスチャのパス
	void loadUiTex(std::wstring keyUiPartsName, std::wstring texPass);

	//UIのノードを組み立てる
	// drawUiNodeType (UI_NODE_NONE_CONF : 設定なし ,UI_NODE_SLIDER_VERTICAL : スライダー縦 , UI_NODE_SLIDER_HORIZON : スライダー横)
	void ComposeUI(DrawUiNodeType drawUiNodeType,std::wstring keyUiName,const std::vector<std::wstring>& uiPartsName);


	//UI描画時の親子関係の構築処理
	// drawUiNodeType (UI_NODE_NONE_CONF : 設定なし ,UI_NODE_SLIDER_VERTICAL : スライダー縦 , UI_NODE_SLIDER_HORIZON : スライダー横)
	// nodeUIName : 木構造のUIの名前
	// nodeName : ノードネーム
	// parentName : 親の名前
	void UiNode(DrawUiNodeType drawUiNodeType,const std::wstring& nodeUIName,const std::wstring& nodeName, const std::wstring& parentName=L"");

	//作成した木構造を表示する
	void DrawTreeUiNode(const std::shared_ptr<UiRelation>& node, int depth = 0);

	//UIパーツボタンを作成する（UV座標をずらしてクリック時のUIに切り替えられるようにしている）
	//keyUiPartsName : UIのパーツ名
	//keyNewUiPartsName : 新しいUIパーツ名（""の時は、keyUiPartsNameこれをキーとして扱う）
	//pWidth : 切り抜くピクセル座標、横幅
	//pHeight : 切り抜くピクセル座標、縦幅
	//pX : 切り抜くピクセル座標＿X
	//pY : 切り抜くピクセル座標＿Y
	void CreateUiPartsButton(std::wstring keyUiPartsName, std::wstring keyNewUiPartsName =L"", float pWidth = 0, float pHeight = 0, float pX = 0, float pY = 0);

	//UIパーツをコピーする
	//keyUiPartsName : コピー元UIのパーツ名
	//keyNewUiPartsName : 新しいUIパーツ名（""の時は、keyUiPartsNameこれをキーとして扱う）
	//pWidth : 切り抜くピクセル座標、横幅
	//pHeight : 切り抜くピクセル座標、縦幅
	//pX : 切り抜くピクセル座標＿X
	//pY : 切り抜くピクセル座標＿Y
	//drawLayer : コピーしたUIパーツの描画順のレイヤー
	void CopyUiParts(std::wstring keyUiPartsName, std::wstring keyNewUiPartsName, float pWidth, float pHeight, float pX, float pY, UIDrawLayer drawLayer = UIDrawLayer::Background);


	//UI上にテキストを描画する（テキストのインデックスを返却する）
	//keyUiPartsName : テキストを描画するUIパーツのキー名
	//idx : 描画するUIのインデックス
	//text : 描画したいテキスト
	//textFormatKey : テキストのフォーマットキー 
	//textColorKey : テキストの描画カラーキー
	// textMoveDirect : テキスト移動設定なし TEXT_NOT_MOVE : テキスト移動しない ,TEXT_MOVE_VERTICAL : テキスト垂直移動 ,TEXT_MOVE_HORIZON : テキスト水平移動
	// setTexBoxSize : テキストの矩形設定どうするか（true = サイズを新しく設定する,false = UIのサイズに合わせる）
	// x: テキスト描画シザーレクト＿X
	// y: テキスト描画シザーレクト＿Y 
	// z: テキスト描画シザーレクト＿Z
	// width  : テキスト矩形横幅
	// height : テキスト矩形縦幅
	//tX : クリッピング範囲内での描画位置＿X
	//tY : クリッピング範囲内での描画位置＿Y
	int DrawTextOnUi(std::wstring keyUiPartsName, int idx, std::wstring text, std::string textFormatKey, std::string textColorKey, TextMoveDirect textMoveDirect = TEXT_MOVE_NO_CONF, bool setTexBoxSize = false, float x=0.0f, float y = 0.0f, float z = 0.0f,
		 float width=0.0f, float height=0.0f, float tX=0.0f, float tY=0.0f);

	//UI上に描画するテキスト上書き
	//keyUiPartsName : テキストを描画するUIパーツのキー名
	//idx : 描画するUIのインデックス
	//idxText : 描画するテキストのインデックス
	//text : 描画したいテキスト
	//textFormatKey : テキストのフォーマットキー 
	//textColorKey : テキストの描画カラーキー
	// textMoveDirect : テキスト移動設定なし TEXT_NOT_MOVE : テキスト移動しない ,TEXT_MOVE_VERTICAL : テキスト垂直移動 ,TEXT_MOVE_HORIZON : テキスト水平移動
	// setTexBoxSize : テキストの矩形設定どうするか（true = サイズを新しく設定する,false = UIのサイズに合わせる）
	// x: テキスト描画シザーレクト＿X
	// y: テキスト描画シザーレクト＿Y 
	// z: テキスト描画シザーレクト＿Z
	// width  : テキスト矩形横幅
	// height : テキスト矩形縦幅
	//tX : クリッピング範囲内での描画位置＿X
	//tY : クリッピング範囲内での描画位置＿Y
	void ReplaceUIText(std::wstring keyUiPartsName, int idx, int idxText,std::wstring text, std::string textFormatKey, std::string textColorKey, TextMoveDirect textMoveDirect = TEXT_MOVE_NO_CONF, bool setTexBoxSize = false, float x = 0.0f, float y = 0.0f, float z = 0.0f,
		float width = 0.0f, float height = 0.0f, float tX = 0.0f, float tY = 0.0f);

	//UIの描画フラグを変更する
	//keyUiPartsName : 対象UIパーツの名前
	//flagDraw : 描画するかどうかのフラグ（true : 描画する / false : 描画しない）
	void ChangeDrawFlag(std::wstring keyUiPartsName, int idx, bool flagDraw);
	
	//テキストの描画フラグを変更する
	//keyUiPartsName : 対象UIパーツの名前
	//idxText : 対象テキストのインデックス
	//flagDraw : 描画するかどうかのフラグ（true : 描画する / false : 描画しない）
	void ChangeTextDrawFlag(std::wstring keyUiPartsName, int idxText, bool flagDraw);


	//描画するインスタンスをくみ上げる
	void BuildInstanceData(const std::wstring& keyUiName);

	//UIイベント処理///////////////////////////////////

	//UIパーツのクリック時の処理
	//keyUiPartsName : UIパーツの名前
	//mouseButton : マウスのどこのボタンか ,Left : 左,Right : 右, Center : 真ん中
	void UiPartsPushButton(std::wstring keyUiPartsName,MouseButton mouseButton);

	//UIをドラッグして動かす（親子関係になっているものだけ）
	// drawUiNodeType (UI_NODE_NONE_CONF : 設定なし ,UI_NODE_SLIDER_VERTICAL : スライダー縦 , UI_NODE_SLIDER_HORIZON : スライダー横)
	//uiName : UIの塊の名前
	//drawKeyUiPartsName : ノードの中のどのUIパーツかを受け取る
	//keyUiPartsName : ドローコールで受け取るUIパーツの名前
	//mouseButton : マウスのどこのボタンか ,Left : 左,Right : 右, Center : 真ん中
	// drawUiPartsType : 描画するUIパーツのタイプ (UI_NONE_CONF 設定なし, UI_BACKGROUND 背景,UI_BUTTON ボタン,	UI_SLIDER_VERTICAL_SCROLLBAR_BACKGROUND スライダー縦のバー背景,UI_SLIDER_VERTICAL_SCROLLBAR スライダー縦のスクロールバー,UI_SLIDER_VERTICAL スライダー 縦, UI_SLIDER_HORIZON_SCROLLBAR_BACKGROUND スライダー横のバー,UI_SLIDER_HORIZON_SCROLLBAR スライダー横のスクロールバー,UI_SLIDER_HORIZON スライダー横)	
	// x : 移動座標＿X
	// y : 移動座標＿Y
	void MoveUi(DrawUiNodeType drawUiNodeType,const std::wstring& uiName,const std::wstring& drawKeyUiPartsName, const std::wstring& keyUiPartsName, MouseButton mouseButton, UiPartMeta uiPartsMeta, float x, float y);

	//UIを指定された座標へと動かす（親子関係になっているものだけ）
	//drawUiNodeType (UI_NODE_NONE_CONF : 設定なし ,UI_NODE_SLIDER_VERTICAL : スライダー縦 , UI_NODE_SLIDER_HORIZON : スライダー横)
	//uiName : UIの塊の名前
	//drawKeyUiPartsName : ノードの中のどのUIパーツかを受け取る
	// x : 移動座標＿X
	// y : 移動座標＿Y
	void MoveUiToPosition(DrawUiNodeType drawUiNodeType, const std::wstring& uiName, const std::wstring& drawKeyUiPartsName, float x, float y);


	//描画位置を変更する
	//keyUiPartsName : 変更するUIキー
	//index : UIのインデックス
	//x : 変更位置
	//y : 変更位置
	//z : 変更位置
	bool SetDrawPosition(const std::wstring& keyUiPartsName, std::size_t index, float x, float y, float z);

	//描画座標を変更する（ノードに適用する）
	//keyUiPartsName : 変更するUIキー
	//index : UIのインデックス
	//node : 参照するノード
	//x : 変更座標
	//y : 変更座標
	void NodeSetDrawPosition(const std::wstring& uiNodeName, const std::shared_ptr<UiRelation>& node, float x, float y);

	//UIを動かす（親子関係になっているものだけ）
	// drawUiNodeType (UI_NODE_NONE_CONF : 設定なし ,UI_NODE_SLIDER_VERTICAL : スライダー縦 , UI_NODE_SLIDER_HORIZON : スライダー横)
	//uiName : UIの塊の名前
	//drawKeyUiPartsName : ノードの中のどのUIパーツかを受け取る
	//keyUiPartsName : ドローコールで受け取るUIパーツの名前
	//mouseButton : マウスのどこのボタンか ,Left : 左,Right : 右, Center : 真ん中
	// drawUiPartsType : 描画するUIパーツのタイプ (UI_NONE_CONF 設定なし, UI_BACKGROUND 背景,UI_BUTTON ボタン,	UI_SLIDER_VERTICAL_SCROLLBAR_BACKGROUND スライダー縦のバー背景,UI_SLIDER_VERTICAL_SCROLLBAR スライダー縦のスクロールバー,UI_SLIDER_VERTICAL スライダー 縦, UI_SLIDER_HORIZON_SCROLLBAR_BACKGROUND スライダー横のバー,UI_SLIDER_HORIZON_SCROLLBAR スライダー横のスクロールバー,UI_SLIDER_HORIZON スライダー横)	
	// x : 移動座標＿X
	// y : 移動座標＿Y
	void MoveUiSlider(DrawUiNodeType drawUiNodeType,const std::wstring& uiName,const std::wstring& drawKeyUiPartsName, const std::wstring& keyUiPartsName, MouseButton mouseButton, SliderPartsRole sliderPartsRole, float x, float y);


	//作成するスライダーの設定
	// drawUiNodeType (UI_NODE_NONE_CONF : 設定なし ,UI_NODE_SLIDER_VERTICAL : スライダー縦 , UI_NODE_SLIDER_HORIZON : スライダー横)
	//keyUiName : UIノード名
	//uiPartsName : キーパーツ名とスライダーでの役割がセットになった配列
	//x : 矩形左上原点座標_X
	//y : 矩形左上原点座標_Y
	//scissorRectsSizeWidth : 矩形サイズ_横幅
	//scissorRectsSizeHeight : 矩形サイズ_縦幅
	//lineNum : 行数
	void CreateUiPartsSlider(DrawUiNodeType drawUiNodeType, std::wstring keyUiName, const std::vector<std::wstring>& uiPartsName, float x, float y, float scissorRectsSizeWidth,float scissorRectsSizeHeight, int totalTextSize=0);

	//文字更新
	void DrawDirectWrite();

	//UIパーツをクリックしたかどうか（クリックすれば0以上のインデックス/してなければ-1）
	int GetUiPartsIdx(std::wstring keyUiPartsName, MouseButton mouseButton);

	//UI上にテキストを検索して特定のUIをクリックしたか、判定する
	//keyUIPartsName : UIパーツの名前
	//uiText : 検索するテキスト
	//mouseButton : マウスのどこのボタンか ,Left : 左,Right : 右, Center : 真ん中
	bool FindClickUIFromText(const std::wstring& keyUiPartsName, const std::wstring& uiText, MouseButton mouseButton);

	//UI上にマウスカーソルあったら選択用の枠表示用
	//keyBaceUiPartsName : ハイライトを表示するベースとなるUIキー名
	//keyHighLightUIPartsName : ハイライトが保存されているUIキー名
	//drawUiPartsType : どのUIパーツでハイライトを適用するか
	//mPos : マウスの座標
	void HighlightUI(const std::wstring& keyBaceUiPartsName, const std::wstring& keyHighLightUIPartsName, DrawUiPartsType drawUiPartsType,const Vec2& mPos);


	//UIアニメーション時間計測開始
	// keyUiPartsName : UI部品の名前の名前キー 
	void StartUiAnim(const std::wstring& keyUiName, UINT idx);

	//originalInstanceとinstanceInfoの値を削除する
	//instanceInfoこの値どれか削除する時originalInstanceこれからも削除する
	//keyUiPartsName : UIパーツ名
	//idx : インデックス
	void DeleteInsAndOrgIns(const std::wstring& keyUiPartsName,UINT idx);

	//UI上にカーソルがあるかどうか探す
	//基底クラスのこの関数で返却する値をセットするため、絶対にUI更新処理でIsHoveringUIこれを呼び出すこと
	//更新前に必ず初期化してからこの関数を実行すること
	void FindUIElementAtCursor(const std::wstring& keyUiPartsName,const Vec2& mPos);
	//////////////////////////////////////////////////



	D3D12_VERTEX_BUFFER_VIEW Getter_uiVbv(std::wstring keyUiPartsName)
	{
		return uiVbv[keyUiPartsName];
	}

	ComPtr<ID3D12Resource> Getter_resUi(std::wstring keyUiPartsName)
	{
		return resUi[keyUiPartsName];
	}



	//UiInfo取得
	//keyUiPartsName : UI部品の名前
	UiInfo& GetterUiInfo(const std::wstring& keyUiPartsName)
	{
		return uiInfo[keyUiPartsName];
	}

	//UiInfo取得
	//keyUiPartsName : UI部品の名前
	const UiInfo& GetterUiInfo(const std::wstring& keyUiPartsName) const
	{
		return uiInfo.at(keyUiPartsName);
	}


	//cbUi取得
	std::unordered_map < std::wstring, ComPtr<ID3D12Resource>> Getter_cbUi()
	{
		return cbUi;
	}

	int Getter_heapIdx(std::wstring keyUiPartsName) {
		return uiContext_.uiHeapIdx[keyUiPartsName];
	}

	D3D12_INDEX_BUFFER_VIEW Getter_uiIbv(std::wstring keyUiPartsName)
	{
		return uiIbv[keyUiPartsName];
	}

	int& GetterDrawNum(std::wstring keyUiPartsName)
	{
		return uiInfo[keyUiPartsName].drawNum;
	}

	const int& GetterDrawNum(std::wstring keyUiPartsName) const
	{
		return uiInfo.at(keyUiPartsName).drawNum;
	}


	std::vector<std::wstring> Getter_drawUiName()
	{
		return drawUiName;
	}

	std::shared_ptr<UiRelation> Getter_uiNode(DrawUiNodeType drawUiNodeType,std::wstring keyUiName,std::wstring keyUiPartsName)
	{
		return uiNode[drawUiNodeType][keyUiName][keyUiPartsName];
	}

	//ゲッターシザーレクト
	//keyUiPartsName : キーUIパーツ名
	CD3DX12_RECT Getter_scissorRect(std::wstring keyUiPartsName) {
		return scissorRect[keyUiPartsName];
	}

	//keyUiPartsName : UIパーツ名
	// drawUiPartsType : 描画するUIパーツのタイプ (UI_NONE_CONF 設定なし, UI_BACKGROUND 背景,UI_BUTTON ボタン,	UI_SLIDER_VERTICAL_BAR スライダー縦のバー,UI_SLIDER_VERTICAL スライダー 縦, UI_SLIDER_HORIZON_BAR スライダー横のバー,UI_SLIDER_HORIZON スライダー横)	
	// idxIns : UIのドロータイプを見つけた時のインデックス
	OriginalInstancePos* FindUiDrawType(std::wstring keyUiPartsName,DrawUiPartsType drawUiPartsType,int* idxIns =nullptr);

	//UI_BACKGROUNDこれがあるか探す
	//keyUiPartsName : UIパーツ名
	// drawUiPartsType : 描画するUIパーツのタイプ (UI_NONE_CONF 設定なし, UI_BACKGROUND 背景,UI_BUTTON ボタン,	UI_SLIDER_VERTICAL_BAR スライダー縦のバー,UI_SLIDER_VERTICAL スライダー 縦, UI_SLIDER_HORIZON_BAR スライダー横のバー,UI_SLIDER_HORIZON スライダー横)	
	// idxIns : UIのドロータイプを見つけた時のインデックス
	OriginalInstancePos* FindSliderRole(std::wstring keyUiPartsName, SliderPartsRole sliderPartsRole, int* idxIns = nullptr);


	Engine::ITextRenderer& GetTextRenderer()
	{

		return uiService_.textRenderer;
	}

	const Engine::ITextRenderer& GetTextRenderer() const
	{
		return uiService_.textRenderer;
	}

	MouseInput& GetMouseInput()
	{
		return uiService_.mouseInput;
	}

	const MouseInput& GetMouseInput() const
	{
		return uiService_.mouseInput;
	}


	CommandContext& GetCommandContext()
	{
		return uiContext_.commandContext;
	}

	const CommandContext& GetCommandContext() const
	{
		return uiContext_.commandContext;
	}

	ComPtr<ID3D12DescriptorHeap>& GetHeap()
	{
		return uiContext_.heap;
	}

	const ComPtr<ID3D12DescriptorHeap>& GetHeap() const
	{
		return uiContext_.heap;
	}

	const ViewportScissor GetViewportScissor()const
	{
		return uiContext_.viewportScissor;
	}


	const UIPipeline& GetUiPipe() const
	{
		return uiContext_.uiPipe;
	}


	std::unordered_map < std::wstring, std::vector<OriginalInstancePos>>& GetOriginalInstancePos()
	{
		return originalInstancePos;
	}


	const std::unordered_map < std::wstring, std::vector<OriginalInstancePos>>& GetOriginalInstancePos() const
	{
		return originalInstancePos;
	}

	//テキスト描画情報を持つ構造体
	struct TextInfo
	{
		float x;	//テキスト描画シザーレクト_X
		float y;	//テキスト描画シザーレクト_Y
		float z;	//テキスト描画シザーレクト_Z

		float originalPosX; //移動前の座標
		float originalPosY;
		float originalPosZ;


		float texBoxWidth; //テキスト矩形横幅
		float texBoxHeight; //テキスト矩形縦幅

		float tX;	//tX : クリッピング範囲内での描画位置＿X
		float tY;	//tY : クリッピング範囲内での描画位置＿Y

		int idxUi; //テキストを描画するUIのインデックス

		std::wstring text;	//描画するテキスト情報
		std::string textFormatKey; //テキストフォーマットキー
		std::string textColorKey; //テキストカラーキー

		TextMoveDirect textMoveDirect; //動かす方向

		bool flagDraw; //描画するかどうか（矩形範囲外では描画しないなど）


		TextInfo()
		{
			x = 0;
			y = 0;
			z = 0;

			originalPosX = 0;
			originalPosY = 0;
			originalPosZ = 0;

			tX = 0;
			tY = 0;

			idxUi = 0;
			text = L"NULL_TEXT";
			textFormatKey = "default";
			textColorKey = "default";
		}
	};

	std::optional<TextInfo> GetTextInfo(const std::wstring& uiNameKey, int textIdx)
	{
		std::optional<TextInfo> textInfomation = std::nullopt;
		auto it = textInfo.find(uiNameKey);
		if (it != textInfo.end()) {
			int size = it->second.size();
			if (textIdx < 0 || textIdx >= size) {
				ThrrowError("TextInfo& GetTextInfo : 配列の要素外を指定しています");
			}

			textInfomation = it->second[textIdx];

		}

		return textInfomation;
	}

	const std::unordered_map < std::wstring, std::vector<TextInfo>>& GetTextInfo() const
	{
		return textInfo;
	}

	std::unordered_map < std::wstring, std::vector<TextInfo>>& GetTextInfo()
	{
		return textInfo;
	}





	std::unordered_map < std::wstring, std::vector<InstanceUi>>& GetInstanceInfo()
	{
		return instanceInfo;
	}

	const std::unordered_map < std::wstring, std::vector<InstanceUi>>& GetInstanceInfo() const
	{
		return instanceInfo;
	}

	std::unordered_map < std::wstring, Slider>& GetSliderInfo()
	{
		return sliderInfo;
	}

	const std::unordered_map < std::wstring, Slider>& GetSliderInfo()const
	{
		return sliderInfo;
	}

	UIHitResult& GetUiHoverHitResult()
	{
		return uiHoverHitResult_;
	}

	const UIHitResult& GetUiHoverHitResult() const
	{
		return uiHoverHitResult_;
	}

	//UIのレイアウト情報を取得する
	const UIPartLayout& GetPartLayouts(const std::wstring& uiPartsName) const
	{
		auto fid = partLayouts_.find(uiPartsName);
		if (fid == partLayouts_.end())
		{
			throw std::runtime_error("UIパーツのレイアウト情報が見つかりませんでした。");
		}
		return partLayouts_.at(uiPartsName);
	}

	//UIの登録したレイアウト順で並び替える
	void SortUIDrawOrder();

private:
	UIHitResult uiHoverHitResult_; //UIヒット結果


	UIService uiService_;
	UIContext uiContext_;

	std::shared_ptr<VertexBufferManager> vertexBufferMgr;
	std::shared_ptr<InstanceBufferManager> instanceBufferManager;
	struct UiStartPos
	{
		bool flagHaveUiStartPos;
		bool flagHaveUiTextStartPos;

		std::vector<DirectX::XMFLOAT4> uiStartPos;
		std::vector<DirectX::XMFLOAT2> uiTextStartPos;

		UiStartPos()
		{
			flagHaveUiStartPos = false;
			flagHaveUiTextStartPos = false;
		}
	};
	std::unordered_map < std::wstring, UiStartPos> uiStartPos;

	//UIのノードタイプを保持
	//string : UIノード名
	std::unordered_map < std::wstring, DrawUiNodeType> uiNodeType;
	//シザーレクト値の保存
	std::unordered_map < std::wstring, CD3DX12_RECT> scissorRect;

	bool flagBackGround = false;

	//スライダーに間する情報持ってる
	//string : UIノードネーム
	std::unordered_map < std::wstring, Slider> sliderInfo;

	struct uiPartsSlider
	{
		float x;
		float y;
		float drawRectWidth;
		float drawRectHeight;
		uiPartsSlider()
		{
			x = 0;
			y = 0;
			drawRectWidth = 0;
			drawRectHeight = 0;
		}
	};

	//string : UIノード名
	std::unordered_map < std::wstring, uiPartsSlider> uiPartsSlider; //クリック時に変化するUV座標を保持


	std::unordered_map < std::wstring, ChangeClickUV> changeUv; //クリック時に変化するUV座標を保持

	std::unordered_map < std::wstring, std::vector<std::wstring>> uiFixName; //クリック時に変化するUV座標を保持


	//string : UIのパーツ名
	std::unordered_map < std::wstring, std::vector<OriginalInstancePos>> originalInstancePos;

	//テキスト描画のインデックス取得
	//string : UIパーツ名
	//std::unordered_map < std::string, int> countText;

	//DrawUiを実行下ときのキー名保存
	std::vector<std::wstring> drawUiName;

	//UIのポリゴン用構造体
	struct UiVertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
	};

	std::unordered_map < std::string, std::vector<UiVertex>> uiV;
	std::unordered_map < std::string, std::vector<int>> indices;

	//新しいUIパーツ名から、もとのUIパーツ名を取得する
	//wstring : 切り抜いた新しいUIの名前
	//wstring : 切り抜く前のUIの名前
	std::unordered_map < std::wstring, std::wstring> uiOriginalData; //切り抜く前のUIの情報

	//UIのキー管理構造体
	struct UiKey
	{
		std::wstring keyUiParts;
		std::wstring keyUiTexPass;
	};
	std::unordered_map < std::wstring, UiKey> uiKey;

	//UIのキーネーム保存用構造体
	//UIの名前重複時や重複時の修正後の名前、元の名前を管理する
	struct RegisterUiKeyName
	{
		int keyCount;	//Uiの重複回数
		std::wstring uiOriginalName; //UI基の名前
		std::wstring fixUiName;		//UI重複修正後の名前
		RegisterUiKeyName()
		{
			keyCount = 0;
			uiOriginalName = L"";
			fixUiName = L"";
		}
	};

	//UIのキーネームを登録する(新しく登録するUI)
	//wstring : UIの塊の名前
	//wstring : UIキーネーム
	//int : 重複した名前の数
	std::unordered_map < std::wstring, std::unordered_map < std::wstring, RegisterUiKeyName>> registerUiKeyName;

	std::unordered_map < std::wstring, UiInfo> uiInfo;

	//UIを木構造で管理
	//DrawUiNodeType : UIノードの描画タイプ設定
	//string : 一塊のUIのキー名
	//string : UIのパーツ名
	std::unordered_map < DrawUiNodeType, std::unordered_map < std::wstring, std::unordered_map < std::wstring,std::shared_ptr<UiRelation>>>> uiNode;


	//string : UIネームキー
	std::unordered_map < std::wstring, std::vector<TextInfo>> textInfo;



	//インスタンス描画関連
	//string : UIのキー
	//InstanceDraw : インスタンス描画データ
	std::unordered_map < std::wstring, std::vector<InstanceUi>> instanceInfo;



	//UIのポリゴンリソース
	std::unordered_map < std::wstring, ComPtr<ID3D12Resource>> resUi;
	//UIのインデックスリソース
	std::unordered_map < std::wstring, ComPtr<ID3D12Resource>> indexRes;
	//インデックスバッファービュー
	std::unordered_map < std::wstring, D3D12_INDEX_BUFFER_VIEW> uiIbv;

	//UIテクスチャ用のリソース
	std::unordered_map < std::wstring, ComPtr<ID3D12Resource>>uiTex;
	std::unordered_map < std::wstring, ComPtr<ID3D12Resource>> cbUi;
	std::unordered_map < std::wstring, D3D12_VERTEX_BUFFER_VIEW> uiVbv;

	UINT incSize;
	UINT sizeCb;



	struct WindowInfo
	{
		int width;
		int height;
	};
	WindowInfo win;

	//定数バッファで用いる
	struct UiDrawPos
	{
		DirectX::XMFLOAT4 pos; //UIの描画位置
		DirectX::XMMATRIX rote; //UIの回転
		DirectX::XMMATRIX orthoMatrix; //UIのピクセル座標からスクリーン座標への変換行列
		DirectX::XMFLOAT4 uvPos; //uv座標
		UiDrawPos()
		{
			uvPos.x = 0; //UV左上
			uvPos.y = 0; //UV左上
			uvPos.w = 1; //UV右下
			uvPos.z = 1; //UV右下
		}

	};
	std::unordered_map < std::wstring, UiDrawPos> uiDrawPos;

	//UIの縦幅横幅
	struct UiTexInfo
	{
		float width;
		float height;


		UiTexInfo()
		{
			width = 0;
			height = 0;
		}
	};
	UiTexInfo uiTexInfo;

	//UIの描画順番を管理する
	std::vector<UIDrawOrder> uiDrawOrderList_;
	//UIの描画順番を管理するための番号
	int uiDrawOrderNum_ = 0;

	//UIのパーツごとのレイアウト情報
	std::unordered_map<std::wstring, UIPartLayout> partLayouts_;

	std::unordered_map<std::wstring,bool> isDraggingMoveUI; //UIをドラッグしているかどうかのフラグ(MoveUI用)

	//UI用のバーテックスバッファとUIのテクスチャリソースビュー作成（座標は、スクリーンスペース座標）
	//keyUiPartsName : UI部品の名前（キー）
	//flagDrawTexSize : テクスチャのサイズで描画(true : テクスチャのサイズで描画 / false : 指定サイズで描画)
	//x : 描画座標＿X
	//y : 描画座標＿Y
	//z  : 画像の表示するZ座標
	//width : UIの幅
	//height : UIの高さ
	//rote   : 回転
	void CreateUiVertexBuffer(std::wstring keyUiPartsName, bool flagDrawTexSize, float x, float y, float z, float width, float height, float rote);



	//テクスチャの中心座標を左上にセットする(NDC変換)
	//x : スクリーン座標＿X
	//y : スクリーン座標＿Y
	void SetUiCenter(float& x, float& y);


	//インスタンシング描画のリソースを作成する
	void MakeResourceInstancing(std::string keyUiPartsName);

	//シザーレクトの設定
	//keyUiPartsName : UIのパーツ名
	//flagSetScissor : flagSetScissor=true シザーレクトの値を指定する / false シザーレクトの値はデフォルトの画面全体
	void SetScissorRect(std::wstring keyUiPartsName, bool flagSetScissor=false,float x=0,float y=0,float width=0,float height=0);

	//UIスライダーの移動処理
	//uiName : UIノードネーム
	//node : UIのノード
	//mx : マウスのX座標
	//my : マウスのY座標
	void MoveUiSliderOnlyText(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node, float& mx, float& my);

	//UIスライダーの移動処理
	//uiName : UIノードネーム
	//node : UIのノード
	//mx : マウスのX座標
	//my : マウスのY座標
	void MoveUiSlider(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node, float& mx, float& my);


	//UI移動時、インスタンス座標更新
	// keyUiParts : UIパーツ名
	// pos : OriginalInstancePos参照渡し
	// instance : InstanceUi参照渡し
	// x : UIのX座標
	// y : UIのY座標
	// z : UIのZ座標
	// mX : 移動量＿X
	// mY : 移動量＿Y
	// idx : instanceInfoのインデックス参照渡し
	// idxUiStrPs : uiStartPosのインデックス参照渡し
	void UpDateMoveInsPos(const std::wstring& keyUiParts, OriginalInstancePos& pos, InstanceUi& instance, float x, float y, float z, float mX, float mY, int& idx, int& idxUiStrPs);

	//UIインデックスから、そのUIが持つテキストをまとめて更新する
	void UpDateMoveTextPos(const std::wstring& keyUiParts, int idxUi, float addTX, float addTY);

	//
	void UpDateMoveTextPos(const std::wstring& uiName, const std::wstring& keyUiParts, float tX, float tY, float tZ, int idx, bool flagUpDateTextPos);

	void UpDateMoveOnlyTextPos(const std::wstring& uiName, const std::wstring& keyUiParts, float tX, float tY, float tZ, int idx, bool flagUpDateTextPos);


	//UI移動関数 UI_NODE_NONE_CONFの場合
	void MoveUi_UI_NODE_NONE_CONF(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node, float mX, float mY);

	//ドラッグ終了したら、UIの開始位置の情報を削除する
	//ノードを辿って、子ノードのUIパーツの開始位置の情報も削除する
	//自分自身も消す
	void EraseUiStartPosRecursive(const std::wstring& uiName, const std::shared_ptr<UiRelation>& node);

	//UIの描画順番を登録する
	void RegisterUIDrawOrder(const std::wstring& keyUiName, UIDrawLayer drawLayer);


};