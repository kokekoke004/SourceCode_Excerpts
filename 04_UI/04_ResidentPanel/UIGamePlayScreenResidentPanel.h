#pragma once
#include"UI.h"
#include<unordered_map>
#include<memory>
#include<string>
#include"CharacterSystem.h"
#include"JobType.h"
#include"ItemMasterIconProvider.h"

namespace Game::Character
{
	class CharacterStatusPresenter;
}

namespace Game::UI::GamePlayScreen::Parts
{
	struct ResidentButtonEntry
	{
		Game::Character::CharacterId characterId;
		std::wstring buttonUiKey;
		std::wstring displayName;
	};

	struct JobButtonEntry
	{
		Game::Character::Job::JobType jobType;
		std::wstring buttonUiKey;
		std::wstring displayName;
	};

	struct SelectResident
	{
		Game::Character::CharacterId currentSelectedResidentId = 0; //現在選択されている入植者のID
		Game::Character::CharacterId oldSelectedResidentId = 0; //前回選択されていた入植者のID

	};


	class UIGamePlayScreenResidentPanel : public UI
	{
	public:
		UIGamePlayScreenResidentPanel(
			UIService& uiService,
			UIContext& uiContext,
			Game::Character::CharacterSystem& characterSystem,
			const Item::Master::ItemMasterIconProvider& itemMasterIconProvider
		);

		~UIGamePlayScreenResidentPanel();

		void LoadResources() override;
		void SetupLayout() override;
		void Initialize() override;
		void Update() override;
		void Draw() override;
		void Exit() override;
		void DrawUIText() override;
		UIHitResult IsHoveringUI() override;

	public:
		std::optional<Game::Character::CharacterId> ConsumeCameraFollowRequest() 
		{
			auto req = cameraFollowRequest_;
			cameraFollowRequest_.reset();
			return req;
		}


	private:
		void LayoutResidentBack();
		//residentButtons_に新しい入植者のボタンを追加する関数
		void CreateResidentButton(const ResidentButtonEntry& entry);

		//jobButtons_に新しい職業のボタンを追加する関数
		void CreateJobButton(const JobButtonEntry& entry);

		//residentButtons_とCharacterSystemのキャラクターデータを同期させる関数
		void SyncResidentButtons();


		//レジデントパネルの詳細上部の描画設定
		void ResidentDetailInfoTopBar(const Game::Character::CharacterId& cId = 0, const Game::Character::Job::JobType& jobType = Game::Character::Job::JobType::None);
		//レジデントパネルの詳細中部の描画設定
		void ResidentDetailInfoMidleBar(const Game::Character::CharacterId& cId = 0);
		//レジデントパネルの詳細下部の描画設定
		void ResidentDetailInfoUnderBar(const Game::Character::CharacterId& cId = 0);

		/*
		* @param アイテムの描画アイコンの位置や種類を切り替える
		*/
		void ChangeDrawItemIcon();

		/*
		* @param 生活状態のテキスト更新
		*/
		void UpdateLivingConditionsText();
	private:
		std::vector<ResidentButtonEntry> residentButtons_;

		std::vector<JobButtonEntry> jobButtons_;

		float UIResidentPanel_Divider_Height = 405.0f; //UIResidentPanel_Dividerの高さ
		float UIResidentPanel_Divider_Side_Width = 940.0f; //UIResidentPanel_Divider_Sideの幅

		int residentDetailInfoTopBarResNameTextIdx_ = -1; //ResidentDetailInfoTopBarで描画するテキストの住民名リソース名のインデックス
		int residentDetailInfoTopBarJobNameTextIdx_ = -1; //ResidentDetailInfoTopBarで描画するテキストの職業名リソース名のインデックス



		//入植者の選択に関する情報をまとめた構造体
		SelectResident selectResident;

	private:
		inline static const std::unordered_map<Game::Character::Job::JobType, std::wstring> jobTable = {
			{Game::Character::Job::JobType::None,L"無職"},
			{Game::Character::Job::JobType::Woodcutter,L"木こり"},
			{Game::Character::Job::JobType::Miner,L"採掘者"},
			{Game::Character::Job::JobType::Builder,L"建築家"},
			{Game::Character::Job::JobType::Farmer,L"農家"},
			{Game::Character::Job::JobType::Gatherer,L"採集者"},
			{Game::Character::Job::JobType::HaulerJob,L"運搬者"},
		};

		struct InventorySlotPos
		{
			float x = 0.0f;
			float y = 0.0f;
		};

		struct ItemIconInfo
		{
			//アイテムのマスターキー
			std::wstring itemMasterKey;
			//アイテムの描画名
			std::wstring itemIconDrawName;
			//アイテムのスタック数
			int itemStackNum = 0;
		};

		enum class LivingConditions
		{
			Hunger,
			Stamina,
			Mood
		};


		struct UiIndexInfo
		{
			int uiIdx = 0;
			int uiTextIdx = 0;
		};

	private:
		Game::Character::CharacterSystem& characterSystem_;
		const Item::Master::ItemMasterIconProvider& itemMasterIconProvider_;
		std::vector<std::wstring> iconNameList_;

		//カメラの追従リクエスト
		std::optional<Game::Character::CharacterId> cameraFollowRequest_;

		//現在NPCが何しているかを示すテキストのUIのインデックス情報
		UiIndexInfo statusTextIdxInfo_;
		//行動テキスト用
		UiIndexInfo actionStatusTextIdxInfo_;


		std::unique_ptr<Game::Character::CharacterStatusPresenter> characterStatusPresenter_;

		//UIのテキストのインデックスを保持するマップ
		//key : LivingConditionsの種類
		//value : UIとテキストのインデックス
		std::unordered_map<LivingConditions, UiIndexInfo> livingConditionsUiTextIdx;


		//アイテムのアイコンの名前のUIパーツ名
		//key : アイテムのテクスチャキー
		//value : UIパーツの名前
		std::unordered_map<std::wstring, std::wstring> uiIconPartsName_;

		//アイコンの配置座標のリスト
		std::vector<InventorySlotPos> inventorySlotPos_;

		//アイテムアイコンの描画リスト
		//key : アイテムマスターキー
		//value : アイテムアイコンの情報
		std::unordered_map<std::wstring, ItemIconInfo> visibleInventoryIconNames_;

		//アイテムのアイコン描画するスロットに対応するUIパーツ名
		//key : スロットのID
		//value : UIパーツ名
		std::unordered_map<int, std::wstring> uiIconDrawSlot_;

	};
}