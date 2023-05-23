// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <Spore\UTFWin\InflateEffect.h>
#include <Spore\UTFWin\ButtonDrawableStandard.h>


const float kBackgroundButtonWidth = 80;
const float kButtonSeparation = 20;
const float kPageArrowWidth = 18;
const uint32_t kBaseButtonControlID = id("MoreBackgroundButtons");

// Make a crude approximation of how many backgrounds we can fit in a page
int CalculateNumItemsPerPage(Editors::PlayModeUI* playModeUI)
{
	auto parentContainer = playModeUI->FindWindowByID(0x044594D8);

	float currentUsedWidth = kPageArrowWidth * 2 + kButtonSeparation * 3 + kBackgroundButtonWidth * 4;
	float totalAvailableWidth = parentContainer->GetRealArea().GetWidth() - 100;
	int numExtraButtons = 0;
	if (totalAvailableWidth > currentUsedWidth) {
		numExtraButtons = (int)((totalAvailableWidth - currentUsedWidth) / (kButtonSeparation + kBackgroundButtonWidth));
	}
	return numExtraButtons + 4;
}

member_detour(EditorPlayMode_HandleUIButton__detour, Editors::EditorPlayMode, bool(uint32_t))
{
	bool detoured(uint32_t buttonID)
	{
		bool result = original_function(this, buttonID);
		if (result)
		{
			int numButtons = CalculateNumItemsPerPage(mpUI.get());
			int numExtraButtons = numButtons - 4;
			for (int i = 0; i < numExtraButtons; i++)
			{
				if (buttonID == kBaseButtonControlID + i)
				{
					uint32_t animID = GetNextEnvironmentReactionAnimID();
					mAnimations.PlayAnimation(mpEditor->mCurrentCreatureID, animID, false, 1);
					// csa_idle_gen_editor
					mAnimations.PlayAnimation(mpEditor->mCurrentCreatureID, 0x4330667, true, 0);
					mpEditor->PostEventToActors(0x70842EF6);
					break;
				}
			}
		}
		return result;
	}
};

virtual_detour(PlayModeUI_HandleUIMessage__detour, Editors::PlayModeUI, UTFWin::IWinProc,
	bool(UTFWin::IWindow*, const UTFWin::Message&))
{
	bool detoured(UTFWin::IWindow* pWindow, const UTFWin::Message& message)
	{
		bool result = original_function(this, pWindow, message);
		if (!result && 
			mMainLayout.IsVisible() && 
			message.IsType(UTFWin::kMsgComponentActivated))
		{
			int numButtons = CalculateNumItemsPerPage(this);
			int numExtraButtons = numButtons - 4;
			for (int i = 0; i < numExtraButtons; i++)
			{
				if (message.ComponentActivated.controlID == kBaseButtonControlID + i)
				{
					Audio::PlayAudio(id("editor_general_click"));
					result = mpPlayMode->HandleUIButton(kBaseButtonControlID + i);
				}
			}
		}
		return result;
	}
};

member_detour(PlayModeBackgrounds_HandleUIButton__detour, Editors::PlayModeBackgrounds, bool(uint32_t))
{
	bool detoured(uint32_t buttonID)
	{
		bool result = original_function(this, buttonID);
		if (!result)
		{
			int numButtons = CalculateNumItemsPerPage(mpPlayModeUI);
			int numExtraButtons = numButtons - 4;
			for (int i = 0; i < numExtraButtons; i++) 
			{
				if (buttonID == kBaseButtonControlID + i) 
				{
					int backgroundIndex = mCurrentPageIndex * numButtons + i + 4;
					if (mTargetIndex != backgroundIndex && backgroundIndex < mBackgrounds.size())
					{
						mTargetIndex = backgroundIndex;
						ToggleBackgroundButtonHighlights(buttonID);
						SwitchBackground();
						result = true;
					}
					break;
				}
			}
		}
		return result;
	}
};

member_detour(PlayModeBackgrounds_UpdateBackgroundButtons__detour, Editors::PlayModeBackgrounds, void())
{
	void detoured()
	{
		int numButtonsPerPage = CalculateNumItemsPerPage(mpPlayModeUI);

		uint32_t buttonIDs[50];
		buttonIDs[0] = 0x445B018;
		buttonIDs[1] = 0x445B318;
		buttonIDs[2] = 0x445B340;
		buttonIDs[3] = 0x445B388;
		for (int i = 4; i < numButtonsPerPage; i++) 
		{
			buttonIDs[i] = kBaseButtonControlID + (i - 4);
		}

		for (int i = 0; i < numButtonsPerPage; i++) 
		{
			if (i >= mBackgrounds.size()) break;

			auto buttonWindow = mpPlayModeUI->FindWindowByID(buttonIDs[i]);
			int backgroundIndex = mCurrentPageIndex * numButtonsPerPage + i;
			if (backgroundIndex < mBackgrounds.size()) 
			{
				auto imageKey = ResourceKey(mBackgrounds[backgroundIndex]->mThumbnailID, TypeIDs::png, 0x100D977C);
				ImagePtr image;
				bool result = UTFWin::Image::GetImage(imageKey, image);
				UTFWin::Image::SetBackground(buttonWindow, image.get(), 0);
			}
			else 
			{
				UTFWin::Image::SetBackground(buttonWindow, nullptr, 0);
			}
		}

		for (int i = mBackgrounds.size(); i < numButtonsPerPage; i++) 
		{
			auto buttonWindow = mpPlayModeUI->FindWindowByID(buttonIDs[i]);
			if (buttonWindow) 
			{
				buttonWindow->SetFlag(UTFWin::kWinFlagVisible, false);
			}
		}
	}
};

member_detour(PlayModeBackgrounds_Load__detour, Editors::PlayModeBackgrounds,
	void(Editors::PlayModeUI*, uint32_t, uint32_t, Graphics::ILightingWorld*, int8_t))
{
	void detoured(Editors::PlayModeUI* playModeUI, uint32_t entryEffectID, uint32_t crossFadeSnapEffectID, Graphics::ILightingWorld* lightingWorld, int8_t backgroundSet)
	{
		mpPlayModeUI = playModeUI;
		mCurrentEntryEffectID = entryEffectID;
		mPlayModeEntryEffectID = entryEffectID;
		mCrossFadeSnapEffectID = crossFadeSnapEffectID;
		mCurrentIndex = 0;
		mTargetIndex = 0;
		field_30 = false;
		mpLightingWorld = lightingWorld;

		LoadBackgroudFiles(backgroundSet);
		mCurrentPageIndex = 0;
		mExtraPageCount = 0;

		int numButtonsPerPage = CalculateNumItemsPerPage(playModeUI);
		int numExtraButtons = numButtonsPerPage - 4;

		auto parentContainer = playModeUI->FindWindowByID(0x044594D8);

		// Move the "Next page" arrow
		auto nextPageArrow = playModeUI->FindWindowByID(0x047ED640);
		auto nextPageArrowArea = Math::Rectangle(nextPageArrow->GetArea());
		nextPageArrowArea.x1 += (kButtonSeparation + kBackgroundButtonWidth) * numExtraButtons;
		nextPageArrowArea.x2 += (kButtonSeparation + kBackgroundButtonWidth) * numExtraButtons;
		nextPageArrow->SetArea(nextPageArrowArea);

		// Move the page text
		auto pagesText = playModeUI->FindWindowByID(0x047ED688);
		auto pagesTextArea = Math::Rectangle(pagesText->GetArea());
		pagesTextArea.x1 += (kButtonSeparation + kBackgroundButtonWidth) * numExtraButtons / 2.0f;
		pagesTextArea.x2 += (kButtonSeparation + kBackgroundButtonWidth) * numExtraButtons / 2.0f;
		pagesText->SetArea(pagesTextArea);

		// Generate the extra buttons

		if (numExtraButtons > 0) {
			// We start where the last button in the SPUI ends
			Math::Rectangle windowCoordinates = { 399.0f, 85.0f, 449.0f, 135.0f };

			for (int i = 0; i < numExtraButtons; i++) {
				windowCoordinates.x1 += kButtonSeparation + kBackgroundButtonWidth;
				windowCoordinates.x2 += kButtonSeparation + kBackgroundButtonWidth;

				auto window = new UTFWin::Window();
				window->SetArea(windowCoordinates);
				window->SetFlag(UTFWin::kWinFlagVisible, true);
				window->SetFlag(UTFWin::kWinFlagIgnoreMouse, true);
				parentContainer->AddWindow(window);

				auto button = object_cast<UTFWin::IButton>(ClassManager.Create(UTFWin::IButton::WinButton_ID, UTFWin::GetAllocator()));
				auto buttonAsWindow = button->ToWindow();
				button->SetButtonType(UTFWin::ButtonTypes::Standard);
				button->SetButtonDrawable(new UTFWin::ButtonDrawableStandard());
				buttonAsWindow->SetArea({ -12.0f, -110.0f, 68.0f, -50.0f });
				buttonAsWindow->SetFlag(UTFWin::kWinFlagVisible, true);
				buttonAsWindow->SetFlag(UTFWin::kWinFlagIgnoreMouse, false);
				buttonAsWindow->SetControlID(kBaseButtonControlID + i);
				buttonAsWindow->AddWinProc(mpPlayModeUI);
				window->AddWindow(buttonAsWindow);

				auto inflateEffect = new UTFWin::InflateEffect();
				inflateEffect->SetTime(0.05f);
				inflateEffect->SetTriggerType(UTFWin::TriggerType::MouseFocus);
				inflateEffect->SetInterpolationType(UTFWin::InterpolationType::EaseInEaseOut);
				inflateEffect->SetEase(0.0f, 0.0f);
				inflateEffect->SetDamping(0.1f);
				inflateEffect->SetScale(1.1f);
				buttonAsWindow->AddWinProc(inflateEffect);
			}
		}

		if (mBackgrounds.size() > 0) {
			mExtraPageCount = (mBackgrounds.size() - 1) / numButtonsPerPage;
		}

		playModeUI->SetWindowVisible(0x47ED4C0, mExtraPageCount > 0);
		UpdatePageNumbers();
		UpdateBackgroundButtons();
		playModeUI->SetWindowVisible(0x459B0F0, true);
		playModeUI->SetWindowVisible(0x459B298, false);
		playModeUI->SetWindowVisible(0x459B2D8, false);
		playModeUI->SetWindowVisible(0x459B348, false);
		mIsSwitchingBackground = false;  // important, without this SwitchBackground() does nothing
	}
};

void Initialize()
{
	// This method is executed when the game starts, before the user interface is shown
	// Here you can do things such as:
	//  - Add new cheats
	//  - Add new simulator classes
	//  - Add new game modes
	//  - Add new space tools
	//  - Change materials
}

void Dispose()
{
	// This method is called when the game is closing
}

void AttachDetours()
{
	// Call the attach() method on any detours you want to add
	// For example: cViewer_SetRenderType_detour::attach(GetAddress(cViewer, SetRenderType));

	PlayModeBackgrounds_Load__detour::attach(GetAddress(Editors::PlayModeBackgrounds, Load));
	PlayModeBackgrounds_UpdateBackgroundButtons__detour::attach(GetAddress(Editors::PlayModeBackgrounds, UpdateBackgroundButtons));
	PlayModeBackgrounds_HandleUIButton__detour::attach(GetAddress(Editors::PlayModeBackgrounds, HandleUIButton));
	PlayModeUI_HandleUIMessage__detour::attach(GetAddress(Editors::PlayModeUI, HandleUIMessage));
	EditorPlayMode_HandleUIButton__detour::attach(GetAddress(Editors::EditorPlayMode, HandleUIButton));
}


// Generally, you don't need to touch any code here
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		ModAPI::AddPostInitFunction(Initialize);
		ModAPI::AddDisposeFunction(Dispose);

		PrepareDetours(hModule);
		AttachDetours();
		CommitDetours();
		break;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

