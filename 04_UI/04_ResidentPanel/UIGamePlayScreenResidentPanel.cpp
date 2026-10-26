#include"UIGamePlayScreenResidentPanel.h"
#include"UIPartsGamePlayScreen.h"
#include"CharacterBase.h"
#include"FbxPos.h"
#include"CharacterSystem.h"
#include"ItemInventory.h"
#include"CharacterStatusPresenter.h"
#include"StaminaNeed.h"

namespace Game::UI::GamePlayScreen::Parts
{
	UIGamePlayScreenResidentPanel::UIGamePlayScreenResidentPanel(
		UIService& uiService,
		UIContext& uiContext,
		Game::Character::CharacterSystem& characterSystem,
		const Item::Master::ItemMasterIconProvider& itemMasterIconProvider)
		: UI(uiService, uiContext), characterSystem_(characterSystem), itemMasterIconProvider_(itemMasterIconProvider)
	{
		GetNowUiScreenKey() = Game::UI::GamePlayParts::ResidentPanel;
		GetChangeUiScreenKey() = GetNowUiScreenKey();

		characterStatusPresenter_.reset(new Game::Character::CharacterStatusPresenter());
	}

	UIGamePlayScreenResidentPanel::~UIGamePlayScreenResidentPanel() = default;

	void UIGamePlayScreenResidentPanel::LoadResources()
	{
		CreateUi(L"UIResidentPanel", L"UI/ColonySimUIResidentPanel.png", false, false, 0, 0, 0, 347, 264, 0);
		CreateUi(L"UIResidentPanel_SimUI", L"UI/ColonySimUI.png", false, false, 0, 0, 0, 347, 264, 0);
		CreateUi(L"UIResidentItemAtlas", L"PrimitiveTexture/ItemAtlas.png", false, false, 0, 0, 0, 100, 100, 0);

	}

	void UIGamePlayScreenResidentPanel::SetupLayout()
	{
		//UIリソース情報の背景UI設定
		{
			ExtractUITex(L"UIResidentPanel", L"UIResidentPanel_BackGround", true, DRAW_SET_SIZE, 727, 350, 0, 379, 1500, 500);
			ExtractUITex(L"UIResidentPanel", L"UIResidentPanel_Divider", true, DRAW_SET_SIZE, 27, 350, 755, 0, 10, UIResidentPanel_Divider_Height);
			ExtractUITex(L"UIResidentPanel", L"UIResidentPanel_Divider_short", true, DRAW_SET_SIZE, 27, 350, 755, 0, 10, UIResidentPanel_Divider_Height / 2);
			ExtractUITex(L"UIResidentPanel", L"UIResidentPanel_Divider_short2", true, DRAW_SET_SIZE, 27, 350, 755, 0, 10, UIResidentPanel_Divider_Height / 2.5);

			ExtractUITex(L"UIResidentPanel", L"UIResidentPanel_Divider_Side", true, DRAW_SET_SIZE, 350, 27, 792, 0, UIResidentPanel_Divider_Side_Width, 10);


			ExtractUITex(L"UIResidentPanel", L"UIResidentPanel_CameraIcon", true, DRAW_SET_SIZE, 247, 228, 0, 731, 50, 50);
			CopyUiParts(L"UIResidentPanel_CameraIcon", L"UIResidentPanel_CameraIconHeighLight", 121, 121, 248, 731);

		}

		//住民一覧のスライダーUI設定
		{
			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_ResidentBack", true, DRAW_SET_SIZE, 369, 60, 0, 98, 385, 60);
			CopyUiParts(L"UIResidentPanel_ResidentBack", L"UIResidentPanel_ResidentBackHeighLight", 354, 90, 0, 440);
			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_Resident_ScrollBar_Back", true, DRAW_SET_SIZE, 79, 410, 99, 539, 50, 380);
			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_Resident_ScrollBar_Button", true, DRAW_SET_SIZE, 47, 80, 0, 713, 40, 66);

		}

		//職業選択ボタンUI設定
		{
			//職業選択ボタン下のバーUI設定
			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_JobBack_UnderBar", true, DRAW_SET_SIZE, 116, 10, 0, 425, 900, 1);

			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_JobBack_ScrollBar_Back", true, DRAW_SET_SIZE, 79, 410, 99, 539, 30, UIResidentPanel_Divider_Height * 0.45);
			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_JobBack_ScrollBar_Button", true, DRAW_SET_SIZE, 47, 80, 0, 713, 40, 40);

			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_JobBack", true, DRAW_SET_SIZE, 369, 60, 0, 98, 250, 50);
			CopyUiParts(L"UIResidentPanel_JobBack", L"UIResidentPanel_JobBackHeighLight", 354, 90, 0, 440);



		}

		//住民パネルのテキスト背景用の枠
		{
			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_TextBack", true, DRAW_SET_SIZE, 369, 60, 0, 98, 250, 40);
			ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_IconBack", true, DRAW_SET_SIZE, 350, 467, 837, 0, 70, 70);

			auto allKeys = itemMasterIconProvider_.GetAllItemKeys();
			for (const auto& key : allKeys) {
				auto iconData = itemMasterIconProvider_.GetIconInfo(key);
				if (iconData && iconData->icon.atlasTexturePath != L"NULL") {
					std::wstring itemIconName = L"UIResidentPanel_IconBack_" + iconData->icon.atlasTextureKey;
					ExtractUITex(L"UIResidentItemAtlas", itemIconName, true, DRAW_SET_SIZE,
						iconData->icon.width, iconData->icon.height, iconData->icon.px, iconData->icon.py, 60, 60);					
					iconNameList_.push_back(itemIconName);

					uiIconPartsName_[iconData->icon.atlasTextureKey] = itemIconName;
				}


			}
		}


		//下のバーUI設定
		ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_UnderBar", true, DRAW_SET_SIZE, 116, 10, 0, 425, 200, 1);

		ExtractUITex(L"UIResidentPanel_SimUI", L"UIResidentPanel_UnderBarLittleLong", true, DRAW_SET_SIZE, 116, 10, 0, 425, 250, 1);

	}

	void UIGamePlayScreenResidentPanel::Initialize()
	{
		UiPartMeta backgroundMeta{};
		backgroundMeta.drawType = UI_BACKGROUND;
		PostDrawUi(L"UIResidentPanel_BackGround", backgroundMeta, 1920/7, 500);

		float UIResidentPanel_BackGround_Width = GetPartLayouts(L"UIResidentPanel_BackGround").width;
		float UIResidentPanel_BackGround_Height = GetPartLayouts(L"UIResidentPanel_BackGround").height;

		UiPartMeta residentCameraIcon{};
		residentCameraIcon.drawType = UI_BUTTON;
		PostDrawUi(L"UIResidentPanel_CameraIcon", residentCameraIcon, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.5, 500 + UIResidentPanel_BackGround_Height * 0.18);
		residentCameraIcon.drawType = UI_NONE_CONF;
		PostDrawUi(L"UIResidentPanel_CameraIconHeighLight", residentCameraIcon, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.5, 500 + UIResidentPanel_BackGround_Height * 0.18, 0.0f,false);


		//UIのレジデントパネル背景描画
		{
			DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"住人管理パネル", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width / 2, 50);

			DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"名前：", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.37, UIResidentPanel_BackGround_Height * 0.22);


			UiPartMeta underBarMeta{};
			underBarMeta.drawType = UI_NONE_CONF;
			PostDrawUi(L"UIResidentPanel_UnderBar", underBarMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.37, 500 + UIResidentPanel_BackGround_Height * 0.26);

			DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"職業：", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.37, UIResidentPanel_BackGround_Height * 0.28);
			PostDrawUi(L"UIResidentPanel_UnderBar", underBarMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.37, 500 + UIResidentPanel_BackGround_Height * 0.32);

			DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"行動：", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.37, UIResidentPanel_BackGround_Height * 0.34);
			
			DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"状態：", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.37, UIResidentPanel_BackGround_Height * 0.4);

			PostDrawUi(L"UIResidentPanel_UnderBar", underBarMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.37, 500 + UIResidentPanel_BackGround_Height * 0.44);

			//これは、UIの背景の上に描画するテキストだからそこに注意
			actionStatusTextIdxInfo_.uiTextIdx = DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"---------", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.41, UIResidentPanel_BackGround_Height * 0.34);
			actionStatusTextIdxInfo_.uiIdx = 0;

			statusTextIdxInfo_.uiTextIdx = DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"---------", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.41, UIResidentPanel_BackGround_Height * 0.4);
			statusTextIdxInfo_.uiIdx = 0;


			PostDrawUi(L"UIResidentPanel_UnderBar", underBarMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.37, 500 + UIResidentPanel_BackGround_Height * 0.38);

			DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"職業リスト", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.4, UIResidentPanel_BackGround_Height * 0.52);
			DrawTextOnUi(L"UIResidentPanel_BackGround", 0, L"職業の説明", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.6, UIResidentPanel_BackGround_Height * 0.52);
		
		
		}

		//職業の所持品などのテキストの背景
		{
			UiPartMeta underBarMeta{};
			underBarMeta.drawType = UI_NONE_CONF;

			int uiIdx = PostDrawUi(L"UIResidentPanel_TextBack", underBarMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.565, 500 + UIResidentPanel_BackGround_Height * 0.17);
			DrawTextOnUi(L"UIResidentPanel_TextBack", uiIdx, L"所持品", "defaultCenter", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, 0, 0);


			GetPartLayouts(L"UIResidentPanel_IconBack").width;

			itemMasterIconProvider_.GetAllItemKeys();

			for (int i = 0; i < 4; i++) {
				uiIdx = PostDrawUi(L"UIResidentPanel_IconBack", underBarMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.552 + i * (GetPartLayouts(L"UIResidentPanel_IconBack").width + 5), 500 + UIResidentPanel_BackGround_Height * 0.28);

			}

			int i = 0;
			for (const auto& iconName : iconNameList_) {
				float x = 1920 / 7 + UIResidentPanel_BackGround_Width * 0.552 + i * (GetPartLayouts(L"UIResidentPanel_IconBack").width + 5);
				float y = 500 + UIResidentPanel_BackGround_Height * 0.28;

				uiIdx = PostDrawUi(iconName, underBarMeta, x, y);
				DrawTextOnUi(iconName, uiIdx, L" ", "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, 0, 0);

				ChangeDrawFlag(iconName, uiIdx, false);
				i++;
				InventorySlotPos inventorySlotPos;
				inventorySlotPos.x = x;
				inventorySlotPos.y = y;
				//アイコンの配置座標をここでリストに格納
				inventorySlotPos_.push_back(inventorySlotPos);
			}


		}

		//生活状態のテキスト関連
		{
			UiPartMeta uiMeta{};
			uiMeta.drawType = UI_NONE_CONF;

			int uiIdx = PostDrawUi(L"UIResidentPanel_TextBack", uiMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.775, 500 + UIResidentPanel_BackGround_Height * 0.17);
			DrawTextOnUi(L"UIResidentPanel_TextBack", uiIdx, L"生活状態", "defaultCenter", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, 0, 0);

			float fontSize = GetFontSize("default");

			//テキストのアンダーバー描画
			uiIdx = PostDrawUi(L"UIResidentPanel_UnderBarLittleLong", uiMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.775, 500 + UIResidentPanel_BackGround_Height * 0.33);
			DrawTextOnUi(L"UIResidentPanel_UnderBarLittleLong", uiIdx, L"空腹度：", "default", "default", TEXT_NOT_MOVE, true, 
				1920 / 7 + UIResidentPanel_BackGround_Width * 0.775, 500 + UIResidentPanel_BackGround_Height * 0.33 - fontSize - 5, 0, 250, fontSize + 5, 20, 0);

			//空腹値の値テキスト
			int textIdx = DrawTextOnUi(L"UIResidentPanel_UnderBarLittleLong", uiIdx, L"---------------------", "default", "default", TEXT_NOT_MOVE, true,
				1920 / 7 + UIResidentPanel_BackGround_Width * 0.82, 500 + UIResidentPanel_BackGround_Height * 0.33 - fontSize - 5, 0, 250, fontSize + 5, 20, 0);
			livingConditionsUiTextIdx[LivingConditions::Hunger] = { uiIdx, textIdx };

			uiIdx = PostDrawUi(L"UIResidentPanel_UnderBarLittleLong", uiMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.775, 500 + UIResidentPanel_BackGround_Height * 0.39);
			DrawTextOnUi(L"UIResidentPanel_UnderBarLittleLong", uiIdx, L"スタミナ：", "default", "default", TEXT_NOT_MOVE, true,
				1920 / 7 + UIResidentPanel_BackGround_Width * 0.775, 500 + UIResidentPanel_BackGround_Height * 0.39 - fontSize - 5, 0, 250, fontSize + 5, 20, 0);

			//スタミナの値テキスト
			textIdx = DrawTextOnUi(L"UIResidentPanel_UnderBarLittleLong", uiIdx, L"未実装", "default", "default", TEXT_NOT_MOVE, true,
				1920 / 7 + UIResidentPanel_BackGround_Width * 0.83, 500 + UIResidentPanel_BackGround_Height * 0.39 - fontSize - 5, 0, 250, fontSize + 5, 20, 0);
			livingConditionsUiTextIdx[LivingConditions::Stamina] = { uiIdx, textIdx };



			uiIdx = PostDrawUi(L"UIResidentPanel_UnderBarLittleLong", uiMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.775, 500 + UIResidentPanel_BackGround_Height * 0.45);
			DrawTextOnUi(L"UIResidentPanel_UnderBarLittleLong", uiIdx, L"気分：", "default", "default", TEXT_NOT_MOVE, true,
				1920 / 7 + UIResidentPanel_BackGround_Width * 0.775, 500 + UIResidentPanel_BackGround_Height * 0.45 - fontSize - 5, 0, 250, fontSize + 5, 20, 0);

			//気分の値テキスト
			textIdx = DrawTextOnUi(L"UIResidentPanel_UnderBarLittleLong", uiIdx, L"未実装", "default", "default", TEXT_NOT_MOVE, true,
				1920 / 7 + UIResidentPanel_BackGround_Width * 0.81, 500 + UIResidentPanel_BackGround_Height * 0.45 - fontSize - 5, 0, 250, fontSize + 5, 20, 0);
			livingConditionsUiTextIdx[LivingConditions::Mood] = { uiIdx, textIdx };

		}

		//入植者の詳細情報の上部
		ResidentDetailInfoTopBar();
		//入植者の詳細情報の中部
		ResidentDetailInfoMidleBar();

		UiPartMeta dividerMeta{};
		dividerMeta.drawType = UI_NONE_CONF;
		PostDrawUi(L"UIResidentPanel_Divider", dividerMeta, 1920 / 7+500, 580);
		PostDrawUi(L"UIResidentPanel_Divider_short", dividerMeta, 1920 / 7 + 500 + UIResidentPanel_Divider_Side_Width/3, 580);
		PostDrawUi(L"UIResidentPanel_Divider_short2", dividerMeta, 1920 / 7 + 500 + UIResidentPanel_Divider_Side_Width / 1.5, 580);

		PostDrawUi(L"UIResidentPanel_Divider_Side", dividerMeta, 1920 / 7 + 500, 580 + UIResidentPanel_Divider_Height * 0.4);

		//PostDrawUi(L"UIResidentPanel_Divider_Side", dividerMeta, 1920 / 7 + 500, 580 + UIResidentPanel_Divider_Height/6);
		PostDrawUi(L"UIResidentPanel_Divider_Side", dividerMeta, 1920 / 7 + 500, 580 + UIResidentPanel_Divider_Height/2);



		//入植者の一覧の背景とスクロールバーの描画
		LayoutResidentBack();

		//職業選択のスクロールバーの描画
		{
			UiPartMeta scrollBarBackMeta{};
			scrollBarBackMeta.sliderRole = SLIDER_VERTICAL_SCROLLBAR_BACKGROUND;
			PostDrawUi(L"UIResidentPanel_JobBack_ScrollBar_Back", scrollBarBackMeta, 1920 / 7 + UIResidentPanel_BackGround_Width *0.90,
				500 + UIResidentPanel_BackGround_Height *0.6);

			UiPartMeta scrollBarMeta{};
			scrollBarMeta.sliderRole = SLIDER_VERTICAL_SCROLLBAR;
			scrollBarMeta.drawType = UI_BUTTON;
			PostDrawUi(L"UIResidentPanel_JobBack_ScrollBar_Button", scrollBarMeta, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.90,
				500 + UIResidentPanel_BackGround_Height * 0.6);

		}

		//入植者の詳細情報の下部
		ResidentDetailInfoUnderBar();

		CreateUiPartsSlider(UI_NODE_SLIDER_VERTICAL, L"UIResidentPanel_Job_ScrollBar", { L"UIResidentPanel_JobBack_ScrollBar_Back", L"UIResidentPanel_JobBack_ScrollBar_Button" , L"UIResidentPanel_JobBack",L"UIResidentPanel_JobBackHeighLight",L"UIResidentPanel_JobBack_UnderBar" },
			1920 / 7 + 500, 500 + UIResidentPanel_BackGround_Height * 0.6,UIResidentPanel_BackGround_Width * 0.90, UIResidentPanel_Divider_Height * 0.45);

	}

	void UIGamePlayScreenResidentPanel::ResidentDetailInfoTopBar(const Game::Character::CharacterId& cId,const Game::Character::Job::JobType& jobType)
	{
		float UIResidentPanel_BackGround_Width = GetPartLayouts(L"UIResidentPanel_BackGround").width;
		float UIResidentPanel_BackGround_Height = GetPartLayouts(L"UIResidentPanel_BackGround").height;

		//入植者IDをもとに、residentButtons_から表示名を取得して、テキストを描画する
		auto it = std::find_if(
			residentButtons_.begin(),
			residentButtons_.end(),
			[&](const ResidentButtonEntry& btn) {
				return btn.characterId == cId;
			});

		//一回DrawTextOnUiでテキストを描画した後は、配列上で確保したテキストに値するインデックスの位置のテキストをReplaceUITextでテキストを更新する
		if (it != residentButtons_.end() && residentDetailInfoTopBarResNameTextIdx_<0) {
			residentDetailInfoTopBarResNameTextIdx_ = DrawTextOnUi(L"UIResidentPanel_BackGround", 0, it->displayName, "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.41, UIResidentPanel_BackGround_Height * 0.22);
		}
		else if (it != residentButtons_.end() && residentDetailInfoTopBarResNameTextIdx_>=0 && selectResident.oldSelectedResidentId!= selectResident.currentSelectedResidentId) {
			ReplaceUIText(L"UIResidentPanel_BackGround", 0, residentDetailInfoTopBarResNameTextIdx_,it->displayName, "default", "default", TEXT_NOT_MOVE, false,
				0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.41, UIResidentPanel_BackGround_Height * 0.22);
		}

		//レジデントパネル上部のアイテムアイコンの描画切り替え
		ChangeDrawItemIcon();

		//入植者IDをもとに、residentButtons_から表示名を取得して、テキストを描画する
		auto itJob = std::find_if(
			jobButtons_.begin(),
			jobButtons_.end(),
			[&](const JobButtonEntry& btn) {
				return btn.jobType == jobType;
			});


		if (itJob != jobButtons_.end() && residentDetailInfoTopBarJobNameTextIdx_ < 0) {
			residentDetailInfoTopBarJobNameTextIdx_ = DrawTextOnUi(L"UIResidentPanel_BackGround", 0, itJob->displayName, "default", "default", TEXT_NOT_MOVE, false, 0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.41, UIResidentPanel_BackGround_Height * 0.28);
		}
		else if (itJob != jobButtons_.end() && residentDetailInfoTopBarJobNameTextIdx_ >= 0) {
			ReplaceUIText(L"UIResidentPanel_BackGround", 0, residentDetailInfoTopBarJobNameTextIdx_, itJob->displayName, "default", "default", TEXT_NOT_MOVE, false,
				0, 0, 0, 0, 0, UIResidentPanel_BackGround_Width * 0.41, UIResidentPanel_BackGround_Height * 0.28);
		}

		if (residentDetailInfoTopBarResNameTextIdx_ >= 0) {
			ChangeTextDrawFlag(L"UIResidentPanel_BackGround", residentDetailInfoTopBarResNameTextIdx_, true);
		}

		if (residentDetailInfoTopBarResNameTextIdx_ >= 0) {
			ChangeTextDrawFlag(L"UIResidentPanel_BackGround", residentDetailInfoTopBarJobNameTextIdx_, true);

		}

		const auto character = characterSystem_.Get(selectResident.currentSelectedResidentId);
		if (character) {
			//キャラクターの状態更新
			std::wstring activityText = characterStatusPresenter_->GetActivityText(*character);

			ReplaceUIText(L"UIResidentPanel_BackGround", 0, actionStatusTextIdxInfo_.uiTextIdx, activityText, "default", "default");

			std::wstring statusText = characterStatusPresenter_->GetStatusText(*character);
			ReplaceUIText(L"UIResidentPanel_BackGround", 0, statusTextIdxInfo_.uiTextIdx, statusText, "default", "default");
		}


		//前回選択されていた入植者IDを現在選択されている入植者IDで上書きする
		selectResident.oldSelectedResidentId = selectResident.currentSelectedResidentId;
	}

	void UIGamePlayScreenResidentPanel::ResidentDetailInfoMidleBar(const Game::Character::CharacterId& cId)
	{
		float UIResidentPanel_BackGround_Width = GetPartLayouts(L"UIResidentPanel_BackGround").width;
		float UIResidentPanel_BackGround_Height = GetPartLayouts(L"UIResidentPanel_BackGround").height;

	}

	void UIGamePlayScreenResidentPanel::ResidentDetailInfoUnderBar(const Game::Character::CharacterId& cId)
	{
		float UIResidentPanel_BackGround_Width = GetPartLayouts(L"UIResidentPanel_BackGround").width;
		float UIResidentPanel_BackGround_Height = GetPartLayouts(L"UIResidentPanel_BackGround").height;


		for (auto& job : jobTable) {
			CreateJobButton({ job.first, L"UIResidentPanel_JobBack", job.second });
		}

	}

	void UIGamePlayScreenResidentPanel::LayoutResidentBack()
	{
		int numResident = 15; //仮の入植者数
		int interval = 60; //入植者UI同士の間隔
		float x = 1920 / 7 + 60; //60はUI同士の隙間


		UiPartMeta scrollBarBackMeta{};
		scrollBarBackMeta.sliderRole = SLIDER_VERTICAL_SCROLLBAR_BACKGROUND;
		PostDrawUi(L"UIResidentPanel_Resident_ScrollBar_Back", scrollBarBackMeta, x + 10 + GetPartLayouts(L"UIResidentPanel_ResidentBack").width - 5, 595);

		UiPartMeta scrollBarMeta{};
		scrollBarMeta.sliderRole = SLIDER_VERTICAL_SCROLLBAR;
		scrollBarMeta.drawType = UI_BUTTON;
		PostDrawUi(L"UIResidentPanel_Resident_ScrollBar_Button", scrollBarMeta, x + 10 + GetPartLayouts(L"UIResidentPanel_ResidentBack").width, 670);

		SyncResidentButtons();
		//CreateUiPartsSlider(UI_NODE_SLIDER_VERTICAL, L"UIResidentPanel_Resident_ScrollBar", { L"UIResidentPanel_Resident_ScrollBar_Back", L"UIResidentPanel_Resident_ScrollBar_Button" , L"UIResidentPanel_ResidentBack",L"UIResidentPanel_ResidentBackHeighLight"}, x, 595, x + GetPartLayouts(L"UIResidentPanel_ResidentBack").width, 380);
	}

	void UIGamePlayScreenResidentPanel::Update()
	{


		auto uiName = Getter_drawUiName();
		std::wstring keyUiName;
		Vec2 mousePos = GetMouseInput().GetMousePos();
		Vec2 movDis = GetMouseInput().GetMouseVelocity();

		std::wstring clickUINameResiBack = L"UIResidentPanel_ResidentBack";
		std::wstring clickUINameJobBack = L"UIResidentPanel_JobBack";

		UiPartMeta uiMetaFilter{};
		uiMetaFilter.sliderRole = SLIDER_VERTICAL_SCROLLBAR;
		uiMetaFilter.drawType = UI_BUTTON;
		//UI上にカーソルがあるかどうかの判定結果をリセット
		//カーソル上に前フレームあったとき、ここで一回初期化
		if (GetUiHoverHitResult().isHit) {
			GetUiHoverHitResult() = {};
		}

		SyncResidentButtons();

		HighlightUI(clickUINameResiBack, L"UIResidentPanel_ResidentBackHeighLight", UI_BUTTON, mousePos);
		HighlightUI(clickUINameJobBack, L"UIResidentPanel_JobBackHeighLight", UI_BUTTON, mousePos);

		HighlightUI(L"UIResidentPanel_CameraIcon", L"UIResidentPanel_CameraIconHeighLight", UI_BUTTON, mousePos);

		auto cameraIdx = GetUiPartsIdx(L"UIResidentPanel_CameraIcon", Left);
		if (cameraIdx >= 0) {
			cameraFollowRequest_ = selectResident.currentSelectedResidentId;
		}

		for (auto uiPartsName : uiName) {
			keyUiName = uiPartsName;


			//カーソルがUI上にあるかどうか
			FindUIElementAtCursor(keyUiName, mousePos);
			//UIResidentPanel_ResidentBackボタンが押されているか
			int idxResi = GetUiPartsIdx(clickUINameResiBack, Left);
			int idxJob = GetUiPartsIdx(clickUINameJobBack, Left);
			//レジデントパネルの住民ボタンが押されたとき、selectedResidentId_にどの住民が選択されたかを保存
			if (idxResi >= 0 && idxResi < static_cast<int>(residentButtons_.size())) {
				selectResident.currentSelectedResidentId = residentButtons_[idxResi].characterId;
			}


			//職業ボタンが押されたとき、選択されている住民の職業を更新する
			if (idxJob >= 0 && idxJob < static_cast<int>(jobButtons_.size())) {
				characterSystem_.SetCharacterJob(selectResident.currentSelectedResidentId,jobButtons_[idxJob].jobType);
			}

			//UIアンダーバーのボタンが押されているとき、currentClickUnderBarButtonにどのボタンが押されたかを保存
			//if (idx >= 0 && idx < static_cast<int>(underBarButtonsTable.size()) && keyUiName == clickUIName) {
			//	//もし、2回目のクリックで同じボタンが押されたら、currentClickUnderBarButtonをリセットする
			//	if (GetCurrentClickUnderBarButton()) {
			//		GetCurrentClickUnderBarButton() = std::nullopt;
			//		auto i = GetCurrentClickUnderBarButton();
			//	}
			//	else {
			//		//初回クリック時にどのボタンが押されたかを保存
			//		GetCurrentClickUnderBarButton() = underBarButtonsTable[idx];
			//	}
			//}


			MoveUi(UI_NODE_SLIDER_VERTICAL, L"UIResidentPanel_Resident_ScrollBar", L"UIResidentPanel_Resident_ScrollBar_Button", keyUiName, Left, uiMetaFilter, movDis.X, movDis.Y);
			MoveUi(UI_NODE_SLIDER_VERTICAL, L"UIResidentPanel_Job_ScrollBar", L"UIResidentPanel_JobBack_ScrollBar_Button", keyUiName, Left, uiMetaFilter, movDis.X, movDis.Y);

		}
		if (selectResident.currentSelectedResidentId) {
			//選択された住民のIDをもとに、ResidentDetailInfoTopBarのテキストを更新する
			auto jobState = characterSystem_.GetJobState(selectResident.currentSelectedResidentId);
			ResidentDetailInfoTopBar(selectResident.currentSelectedResidentId, jobState.type);
			UpdateLivingConditionsText();
		}


		for (auto uiPartsName : uiName) {
			BuildInstanceData(uiPartsName);  //全て今フレームのデータ
		}
	}
	void UIGamePlayScreenResidentPanel::Draw()
	{
		DrawCommon();
	}

	void UIGamePlayScreenResidentPanel::Exit()
	{

	}

	void UIGamePlayScreenResidentPanel::DrawUIText()
	{
		DrawDirectWrite();
	}

	UIHitResult UIGamePlayScreenResidentPanel::IsHoveringUI()
	{
		return GetUiHoverHitResult();
	}

	void UIGamePlayScreenResidentPanel::SyncResidentButtons()
	{
		std::vector<Game::Character::CharacterId> currentIds;
		int oldSize = residentButtons_.size();

		for (const auto& e : characterSystem_.GetAllData()) {
			if (!e.value.ch) continue;
			if (e.value.life != Game::Character::CharacterSlot::LifeState::Alive || e.value.entityType != EntityType::NPC) continue;
			currentIds.push_back(e.key);

			//ベクターのキャラクターIDと同じものがresidentButtons_これに既に登録されてるか見る
			auto it = std::find_if(
				residentButtons_.begin(),
				residentButtons_.end(),
				[&](const ResidentButtonEntry& btn) {
					return btn.characterId == e.key;
				});

			//登録されていなければresidentButtons_に追加する
			if (it == residentButtons_.end()) {
				ResidentButtonEntry entry{};
				entry.characterId = e.key;
				entry.buttonUiKey = L"UIResidentPanel_ResidentBack";
				entry.displayName = e.value.displayName;

				residentButtons_.push_back(entry);

				//新規UIボタン生成
				CreateResidentButton(entry);
			}
			else {
				it->displayName = e.value.displayName;
			}
		}

		//消えた住人を削除
		//remove_if自体は実際には削除せず、残す要素を前に詰めて新しい終端を返す
		//その新しい終端から元のend()までをeraseでまとめて削除する
		residentButtons_.erase(
			std::remove_if(
				residentButtons_.begin(),
				residentButtons_.end(),
				[&](const ResidentButtonEntry& btn) {
					return std::find(currentIds.begin(), currentIds.end(), btn.characterId) == currentIds.end();
				}),
			residentButtons_.end());

		//UIの数が変わったらスクロールバーを再配置する
		if (oldSize != residentButtons_.size()) {
			float x = 1920 / 7 + 60; //60はUI同士の隙間
			CreateUiPartsSlider(UI_NODE_SLIDER_VERTICAL, L"UIResidentPanel_Resident_ScrollBar", { L"UIResidentPanel_Resident_ScrollBar_Back", L"UIResidentPanel_Resident_ScrollBar_Button" , L"UIResidentPanel_ResidentBack",L"UIResidentPanel_ResidentBackHeighLight" }, x, 595, x + GetPartLayouts(L"UIResidentPanel_ResidentBack").width, 380);
		}
	}



	void UIGamePlayScreenResidentPanel::CreateResidentButton(const ResidentButtonEntry& entry)
	{
		int size = residentButtons_.size();
		int idx = size - 1;
		int numResident = 15; //仮の入植者数
		int interval = 60; //入植者UI同士の間隔
		float x = 1920 / 7 + 60; //60はUI同士の隙間
		float y = 600 + idx * interval;

		UiPartMeta residentBackMeta{};
		residentBackMeta.drawType = UI_BUTTON;
		residentBackMeta.sliderRole = SLIDER_VERTICAL;
		PostDrawUi(L"UIResidentPanel_ResidentBack", residentBackMeta, x, y);
		DrawTextOnUi(L"UIResidentPanel_ResidentBack", idx, entry.displayName, "default", "default", TEXT_MOVE_VERTICAL, false, 0, 0, 0, 0, 0, 50, 25);

		PostDrawUi(L"UIResidentPanel_ResidentBackHeighLight", residentBackMeta, x, y);



	}

	void UIGamePlayScreenResidentPanel::CreateJobButton(const JobButtonEntry& entry)
	{
		for (const auto& job : jobTable) {
			if (job.first == entry.jobType) {
			}
		}

		auto it = std::find_if(
			jobButtons_.begin(),
			jobButtons_.end(),
			[&](const JobButtonEntry& btn) {
				return btn.jobType == entry.jobType;
			});

		if (it != jobButtons_.end()) {
			//既に登録されていれば、ここで関数終了
			return;
		}

		JobButtonEntry jobEntry{};
		jobEntry.jobType = entry.jobType;
		jobEntry.buttonUiKey = L"UIResidentPanel_JobBack";
		jobEntry.displayName = entry.displayName;
		jobButtons_.push_back(jobEntry);


		float UIResidentPanel_BackGround_Width = GetPartLayouts(L"UIResidentPanel_BackGround").width;
		float UIResidentPanel_BackGround_Height = GetPartLayouts(L"UIResidentPanel_BackGround").height;
		int size = jobButtons_.size();
		int idx = size - 1;


		PostDrawUi(L"UIResidentPanel_JobBack", { UI_BUTTON,SLIDER_VERTICAL }, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.36, 500 + UIResidentPanel_BackGround_Height * (0.6 + idx * 0.12));
		DrawTextOnUi(L"UIResidentPanel_JobBack", idx, entry.displayName, "default", "default", TEXT_MOVE_VERTICAL, false, 0, 0, 0, 0, 0, 250 * 0.37, 50 * 0.28);
		DrawTextOnUi(L"UIResidentPanel_JobBack", idx, L"職業の説明" + std::to_wstring(idx), "default", "default", TEXT_MOVE_VERTICAL, true, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.6, 500 + UIResidentPanel_BackGround_Height * (0.6 + idx * 0.12), 0,
			UIResidentPanel_BackGround_Width * 0.4, UIResidentPanel_BackGround_Height * 0.3, 0, 20);

		PostDrawUi(L"UIResidentPanel_JobBackHeighLight", { UI_BUTTON,SLIDER_VERTICAL }, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.36, 500 + UIResidentPanel_BackGround_Height * (0.6 + idx * 0.12));
		PostDrawUi(L"UIResidentPanel_JobBack_UnderBar", { UI_BUTTON,SLIDER_VERTICAL }, 1920 / 7 + UIResidentPanel_BackGround_Width * 0.36, 500 + UIResidentPanel_BackGround_Height * (0.71 + idx * 0.12));

	}

	void UIGamePlayScreenResidentPanel::ChangeDrawItemIcon()
	{
		//配置座標を決め直すか
		bool isResetPlace = false;
		if (selectResident.currentSelectedResidentId != 0 && selectResident.oldSelectedResidentId != selectResident.currentSelectedResidentId) {
			isResetPlace = true;
		}

		auto chInv = characterSystem_.GetInventory(selectResident.currentSelectedResidentId);
		if (chInv) {
			int idx = 0;
			auto itemDatas = chInv->GetAllData();

			//全て空っぽになったら描画全て解除
			if (itemDatas.empty()) {
				//前回アイコン表示していたものを非表示にする
				for (const auto& [key, iconData] : visibleInventoryIconNames_) {
					ChangeDrawFlag(iconData.itemIconDrawName, 0, false);
				}
				visibleInventoryIconNames_.clear();
				uiIconDrawSlot_.clear();
				return;
			}

			//再配置するかどうか判断するための走査
			for (const auto& itemData : itemDatas) {
				auto it = uiIconPartsName_.find(itemData.value.masterKey);
				if (it == uiIconPartsName_.end()) {
					continue;
				}

				if (inventorySlotPos_.size() <= idx) {
					break;
				}

				auto iconPartsName = uiIconDrawSlot_.find(idx);
				//アイコンを描画するスロットで描画したいものが現在指定したいものと違えば、アイテムが所持品から無くなってるから以前描画していたものを描画しないようにする
				if (iconPartsName == uiIconDrawSlot_.end() || iconPartsName->second != it->second) {
					isResetPlace = true;
				}

				//アイテムの個数が変わった時
				auto visible = visibleInventoryIconNames_.find(itemData.value.masterKey);
				if (visible == visibleInventoryIconNames_.end() || visible->second.itemStackNum != itemData.value.stackCount) {
					isResetPlace = true;
				}
				idx++;
			}

			//以前よりもアイテムの数が減っていた時
			if (idx < static_cast<int>(uiIconDrawSlot_.size())) {
				isResetPlace = true;
			}

			//ここまででフラグ立っていなければここで終了
			if (!isResetPlace) {
				return;
			}

			//前回表示していたものを一回非表示にする
			for (const auto& [slot, iconName] : uiIconDrawSlot_) {
				ChangeDrawFlag(iconName, 0, false);
			}

			visibleInventoryIconNames_.clear();
			uiIconDrawSlot_.clear();

			idx = 0;

			//再配置していく
			for (const auto& itemData : itemDatas) {
				auto it = uiIconPartsName_.find(itemData.value.masterKey);
				if (it == uiIconPartsName_.end()) {
					continue;
				}

				if (inventorySlotPos_.size() <= idx) {
					break;
				}

				auto itemIconData = itemMasterIconProvider_.GetIconInfo(itemData.value.masterKey);
				if (itemIconData == std::nullopt) {
					continue;
				}
				//描画切り替え
				ChangeDrawFlag(it->second, 0, true);
				std::wstring text = itemIconData->displayInfo.displayName + std::to_wstring(itemData.value.stackCount);
				float textXOffset = CalculateCenteredTextXOffset("default", text, 60);

				ReplaceUIText(it->second, 0, 0, text, "default", "default");
				SetTextPosition(it->second, 0, textXOffset, 40);
				
				
				//配置座標変更
				SetDrawPosition(it->second, 0, inventorySlotPos_[idx].x, inventorySlotPos_[idx].y, 0);
				ItemIconInfo itemIconInfo;
				itemIconInfo.itemIconDrawName = it->second;
				itemIconInfo.itemMasterKey = itemData.value.masterKey;
				itemIconInfo.itemStackNum = itemData.value.stackCount;

				visibleInventoryIconNames_[itemData.value.masterKey] = itemIconInfo;
				uiIconDrawSlot_[idx] = it->second;

				idx++;
			}

			isResetPlace = false;

		}
		//インベントリが取れない住民を選んだ時の処理
		//前回描画を全て非表示に
		else {
			for (const auto& [slot, iconName] : uiIconDrawSlot_) {
				ChangeDrawFlag(iconName, 0, false);
			}
			visibleInventoryIconNames_.clear();
			uiIconDrawSlot_.clear();
		}


	}

	void UIGamePlayScreenResidentPanel::UpdateLivingConditionsText()
	{
		//空腹値の値テキストの更新
		int uiIdx = livingConditionsUiTextIdx[LivingConditions::Hunger].uiIdx;
		int uiTextIdx = livingConditionsUiTextIdx[LivingConditions::Hunger].uiTextIdx;

		auto resident = characterSystem_.Get(selectResident.currentSelectedResidentId);

		if (!resident) {
			return;
		}
		auto hungerNeed = resident->GetCharacterNeeds().hungerNeed;

		float hunger = hungerNeed.GetValue();
		float maxHunger = hungerNeed.GetMaxValue();
		ReplaceUIText(L"UIResidentPanel_UnderBarLittleLong", uiIdx, uiTextIdx, std::to_wstring(static_cast<int>(hunger)) + L"/" + std::to_wstring(static_cast<int>(maxHunger)), "default", "default");


		//スタミナの値テキストの更新
		uiIdx = livingConditionsUiTextIdx[LivingConditions::Stamina].uiIdx;
		uiTextIdx = livingConditionsUiTextIdx[LivingConditions::Stamina].uiTextIdx;

		auto stamina = resident->GetCharacterNeeds().staminaNeed;

		float staminaValue = stamina.GetValue();
		float maxStamina = stamina.GetMaxValue();

		ReplaceUIText(L"UIResidentPanel_UnderBarLittleLong", uiIdx, uiTextIdx, std::to_wstring(static_cast<int>(staminaValue)) + L"/" + std::to_wstring(static_cast<int>(maxStamina)), "default", "default");


	}

}