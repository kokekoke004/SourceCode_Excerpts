#pragma once

#include"Uiutillity.h"

//UIクラスの基底クラス
//UIScreen<-UI<=作りたいUIのクラス
//このような構成

struct UIHitResult
{
    bool isHit = false;
    UIScreenKey uiScreenPartsKey = L"";
	std::wstring drawUiPartsName = L"";
    int index = -1;
};


class IUIScreen {
public:
    virtual ~IUIScreen() = default;

    //テクスチャなどのリソースのロード
    virtual void LoadResources() = 0;
    //UI描画位置などの決定まで
    virtual void SetupLayout() = 0;
    //数値関係の初期化処理
    virtual void Initialize() = 0;
    //UIの位置を更新したり
    virtual void Update() = 0;
    //UI描画
    virtual void Draw() = 0;
    //
    virtual void Exit() = 0;
    //UIに文字を描画する
    virtual void DrawUIText()=0;
	//UI上にカーソルがあるか
	virtual UIHitResult IsHoveringUI() = 0;

    UIScreenKey& GetChangeUiScreenKey()
    {
        return changeUiScreenKey;
    }

    const UIScreenKey& GetChangeUiScreenKey() const
    {
        return changeUiScreenKey;
    }

    UIScreenKey& GetNowUiScreenKey()
    {
        return nowUiScreenKey_;
    }

    const UIScreenKey& GetNowUiScreenKey() const
    {
        return nowUiScreenKey_;
    }



private:
    UIScreenKey changeUiScreenKey;
    UIScreenKey nowUiScreenKey_;
};