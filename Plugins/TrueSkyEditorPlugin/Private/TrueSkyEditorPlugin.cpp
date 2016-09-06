// Copyright 2013-2015 Simul Software Ltd. All Rights Reserved.

#include "TrueSkyEditorPluginPrivatePCH.h"

#include "TrueSkySequenceActor.h"
#if UE_EDITOR
#include "Editor.h"

#include "LevelEditor.h"
#include "IMainFrameModule.h"
#include "IDocumentation.h"
#include "AssetToolsModule.h"
#endif

#include "GenericWindow.h"
#include "WindowsWindow.h"
#include "RendererInterface.h"
#include "UnrealClient.h"
#include "SDockTab.h"

DEFINE_LOG_CATEGORY_STATIC(TrueSky, Log, All);

// Dependencies.
#include "Core.h"
#include "GPUProfiler.h"
#include "ShaderCore.h"
#include "Engine.h"
#include "StaticArray.h"
#include "ActorCrossThreadProperties.h"

#include "Tickable.h"
#include "AssetTypeActions_TrueSkySequence.h"

#include "EngineModule.h"
#include <string>

using namespace simul;

#define ENABLE_AUTO_SAVING
#define DECLARE_TOGGLE(name)\
	void					OnToggle##name();\
	bool					IsToggled##name();

#define IMPLEMENT_TOGGLE(name)\
	void FTrueSkyEditorPlugin::OnToggle##name()\
{\
	ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();\
	bool c=trueSkyPlugin.GetRenderBool(#name);\
	trueSkyPlugin.SetRenderBool(#name,!c);\
	GEditor->RedrawLevelEditingViewports();\
}\
\
bool FTrueSkyEditorPlugin::IsToggled##name()\
{\
	ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();\
	return trueSkyPlugin.GetRenderBool(#name);\
}

#define DECLARE_ACTION(name)\
	void					OnTrigger##name()\
	{\
		ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();\
		trueSkyPlugin.TriggerAction(#name);\
		GEditor->RedrawLevelEditingViewports();\
	}


class FTrueSkyTickable : public  FTickableGameObject
{
public:
	/** Tick interface */
	void					Tick( float DeltaTime );
	bool					IsTickable() const;
	TStatId					GetStatId() const;
};

class FTrueSkyEditorPlugin : public ITrueSkyEditorPlugin
#ifdef SHARED_FROM_THIS
	, public TSharedFromThis<FTrueSkyEditorPlugin,(ESPMode::Type)0>
#endif
{
public:
	FTrueSkyEditorPlugin();
	virtual ~FTrueSkyEditorPlugin();

	static FTrueSkyEditorPlugin*	Instance;
	
	void					OnDebugTrueSky(class UCanvas* Canvas, APlayerController*);

	/** IModuleInterface implementation */
	virtual void			StartupModule() override;
	virtual void			ShutdownModule() override;
	virtual bool			SupportsDynamicReloading() override;
	
#if UE_EDITOR
	/** TrueSKY menu */
	void					FillMenu( FMenuBuilder& MenuBuilder );
	/** Overlay sub-menu */
	void					FillOverlayMenu(FMenuBuilder& MenuBuilder);
#endif
	/** Open editor */
	virtual void			OpenEditor(UTrueSkySequenceAsset* const TrueSkySequence);

#if UE_EDITOR
	struct SEditorInstance
	{
		/** Editor window */
		TSharedPtr<SWindow>		EditorWindow;
		/** Dockable tab */
		TSharedPtr< SDockableTab > DockableTab;
		/** Editor widow HWND */
		HWND					EditorWindowHWND;
		/** Original window message procedure */
		WNDPROC					OrigEditorWindowWndProc;
		/** Asset in editing */
		TWeakObjectPtr<UTrueSkySequenceAsset> Asset;
		/** ctor */
//		SEditorInstance() : EditorWindow(NULL), EditorWindowHWND(0), OrigEditorWindowWndProc(NULL), Asset(NULL) {}
		/** Saves current sequence data into Asset */
		void					SaveSequenceData();
		/** Loads current Asset data into Environment */
		void					LoadSequenceData();
	};

	SEditorInstance*		FindEditorInstance(const TSharedRef<SWindow>& EditorWindow);
	SEditorInstance*		FindEditorInstance(HWND const EditorWindowHWND);
	SEditorInstance*		FindEditorInstance(UTrueSkySequenceAsset* const Asset);
	int						FindEditorInstance(SEditorInstance* const Instance);
	/** Saves all Environments */
	void					SaveAllEditorInstances();
#endif
	void					Tick( float DeltaTime );

#if UE_EDITOR
	virtual void			SetUIString(SEditorInstance* const EditorInstance,const char* name, const char*  value);
#endif
	virtual void			SetCloudShadowRenderTarget(FRenderTarget *t);
protected:
	
	void					OnMainWindowClosed(const TSharedRef<SWindow>& Window);

	/** Called when Toggle rendering button is pressed */
	void					OnToggleRendering();
	/** Returns true if Toggle rendering button should be enabled */
	bool					IsToggleRenderingEnabled();
	/** Returns true if Toggle rendering button should be checked */
	bool					IsToggleRenderingChecked();
	/** Always returns true */
	bool					IsMenuEnabled();

	/** Called when the Toggle Fades Overlay button is pressed*/
	DECLARE_TOGGLE(ShowFades)
	DECLARE_TOGGLE(ShowCubemaps)
	DECLARE_TOGGLE(ShowLightningOverlay)
	DECLARE_TOGGLE(ShowCompositing)
	DECLARE_TOGGLE(ShowCelestialDisplay)
	DECLARE_TOGGLE(OnScreenProfiling)
	DECLARE_TOGGLE(Show3DCloudTextures)
	DECLARE_TOGGLE(Show2DCloudTextures)
	void OnTriggerRecompileShaders()
	{
		ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
		if(!trueSkyPlugin.TriggerAction("RecompileShaders"))
		{
			FText txt=FText::FromString(TEXT("Cannot recompile trueSKY shaders"));
			FMessageDialog::Open(EAppMsgType::Ok,txt,nullptr);
		}
		GEditor->RedrawLevelEditingViewports();
	}
	DECLARE_ACTION(CycleCompositingView)
	void ShowDocumentation()
	{
		FString DocumentationUrl="http://docs.simul.co/unrealengine";
		FPlatformProcess::LaunchURL(*DocumentationUrl, NULL, NULL);
	}
	void ExportCloudLayer();
	
	/** Adds a TrueSkySequenceActor to the current scene */
	void					OnAddSequence();
	void					DeployDefaultContent();
	void					OnSequenceDestroyed();
	/** Returns true if user can add a sequence actor */
	bool					IsAddSequenceEnabled();

	/** Initializes all necessary paths */
	void					InitPaths();
	
#if UE_EDITOR
	/** Creates new instance of UI */
	SEditorInstance*		CreateEditorInstance(  void* Env );
	TSharedPtr< FExtender > MenuExtender;
	TSharedPtr<FAssetTypeActions_TrueSkySequence> SequenceAssetTypeActions;
#endif
	typedef void (*FOpenUI)(HWND, RECT*, RECT*, void*,PluginStyle style,const char *skin);
	typedef void (*FCloseUI)(HWND);
	
	typedef void (*FSetView)(int view_id,const float *view,const float *proj);
	typedef void (*FStaticSetUIString)(HWND,const char* name,const char* value );

	typedef void (*FPushStyleSheetPath)(const char*);
	typedef void (*FSetSequence)(HWND, const char*,int length_hint);
	typedef char* (*FAlloc)(int size);
	typedef char* (*FGetSequence)(HWND, FAlloc);
	typedef void (*FOnTimeChangedCallback)(HWND,float);
	typedef void (*FSetOnTimeChangedInUICallback)(FOnTimeChangedCallback);
	typedef void (*FTrueSkyUILogCallback)(const char *);
	typedef void (*FSetTrueSkyUILogCallback)(FTrueSkyUILogCallback);
	typedef void (*FOnSequenceChangeCallback)(HWND,const char *);
	typedef void (*FSetOnPropertiesChangedCallback)( FOnSequenceChangeCallback);

	typedef int (*FStaticInitInterface)(  );
	typedef int (*FStaticPushPath)(const char*,const char*);
	typedef int (*FStaticGetOrAddView)( void *);

	typedef int (*FStaticOnDeviceChanged)( void * device );
	typedef class UE4PluginRenderInterface* (*FStaticGetRenderInterfaceInstance)();
	typedef void (*FStaticTriggerAction)( const char* name );
	typedef void (*FStaticEnableUILogging)( const char* name );

	
	typedef bool (__stdcall *FGetColourTableCallback)(unsigned,int,int,int,float *);
	typedef void (*FSetGetColourTableCallback)(FGetColourTableCallback);
	
	FOpenUI								OpenUI;
	FCloseUI							CloseUI;
	FSetView							SetView;
	FStaticSetUIString					StaticSetUIString;
	FPushStyleSheetPath					PushStyleSheetPath;
	FSetSequence						SetSequence;
	FGetSequence						GetSequence;
	FSetOnPropertiesChangedCallback		SetOnPropertiesChangedCallback;
	FSetOnTimeChangedInUICallback		SetOnTimeChangedInUICallback;
	FStaticInitInterface				StaticInitInterface;
	FStaticPushPath						StaticPushPath;
	FStaticGetOrAddView					StaticGetOrAddView;
	FStaticOnDeviceChanged				StaticOnDeviceChanged;
	FStaticTriggerAction				StaticTriggerAction;
	FStaticEnableUILogging				StaticEnableUILogging;
	FSetTrueSkyUILogCallback			SetTrueSkyUILogCallback;
	FSetGetColourTableCallback			SetGetColourTableCallback;
	const TCHAR*					PathEnv;

	bool					RenderingEnabled;
	bool					RendererInitialized;

	float					CachedDeltaSeconds;
	float					AutoSaveTimer;		// 0.0f == no auto-saving
	

	TArray<SEditorInstance>	EditorInstances;

	static LRESULT CALLBACK EditorWindowWndProc(HWND, ::UINT, WPARAM, LPARAM);
	static ::UINT			MessageId;

	static void				OnSequenceChangeCallback(HWND OwnerHWND,const char *);
	static void				TrueSkyUILogCallback(const char *);
	static void				OnTimeChangedCallback(HWND OwnerHWND,float t);
	static bool				GetColourTableCallback(unsigned,int,int,int,float *target);
	static void				RetrieveColourTables();
	FRenderTarget			*cloudShadowRenderTarget;

	bool					haveEditor;
};

IMPLEMENT_MODULE( FTrueSkyEditorPlugin, TrueSkyEditorPlugin )

#define LOCTEXT_NAMESPACE "Simul"

#if UE_EDITOR
class FTrueSkyCommands : public TCommands<FTrueSkyCommands>
{
public:
	//	TCommands( const FName InContextName, const FText& InContextDesc, const FName InContextParent, const FName InStyleSetName )
	FTrueSkyCommands()
		: TCommands<FTrueSkyCommands>(
		TEXT("TrueSky"), // Context name for fast lookup
		NSLOCTEXT("Contexts", "TrueSkyCmd", "Simul TrueSky"), // Localized context name for displaying
		NAME_None, // Parent context name. 
		FEditorStyle::GetStyleSetName()) // Parent
	{
	}
	virtual void RegisterCommands() override
	{
		UI_COMMAND(AddSequence					,"Add Sequence To Scene"	,"Adds a TrueSkySequenceActor to the current scene", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(ToggleShowCelestialDisplay	,"Celestial Display"		,"Toggles the celestial display.", EUserInterfaceActionType::ToggleButton, FInputGesture());
		UI_COMMAND(ToggleOnScreenProfiling		,"Profiling"				,"Toggles the Profiling display.", EUserInterfaceActionType::ToggleButton, FInputGesture());
		UI_COMMAND(ToggleShowFades				,"Atmospheric Tables"		,"Toggles the atmospheric tables overlay.", EUserInterfaceActionType::ToggleButton, FInputGesture());
		UI_COMMAND(ToggleShowCubemaps			,"Cubemaps"					,"Toggles the cubemaps overlay.", EUserInterfaceActionType::ToggleButton, FInputGesture());
		UI_COMMAND(ToggleShowLightningOverlay	,"Lightning"				,"Toggles the lightning overlay.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::L));
		UI_COMMAND(ToggleShowCompositing		,"Compositing"				,"Toggles the compositing overlay.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Alt, EKeys::O));
		UI_COMMAND(TriggerCycleCompositingView	,"Cycle Compositing View"	,"Changes which view is shown on the compositing overlay.", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(ToggleShow3DCloudTextures	,"Show 3D Cloud Textures"	,"Toggles the 3D cloud overlay.", EUserInterfaceActionType::ToggleButton, FInputGesture());
		UI_COMMAND(ToggleShow2DCloudTextures	,"Show 2D Cloud Textures"	,"Toggles the 2D cloud overlay.", EUserInterfaceActionType::ToggleButton, FInputGesture());
		UI_COMMAND(TriggerRecompileShaders		,"Recompile Shaders"		,"Recompiles the shaders."		, EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::R));
		UI_COMMAND(TriggerShowDocumentation		,"trueSKY Documentation..."	,"Shows the trueSKY help pages.", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(TriggerExportCloudLayer		,"Export Cloud Layer..."	,"Export to a fbx file."		, EUserInterfaceActionType::Button, FInputGesture());
	}
public:
	TSharedPtr<FUICommandInfo> AddSequence;
	TSharedPtr<FUICommandInfo> ToggleShowFades;
	TSharedPtr<FUICommandInfo> ToggleShowCubemaps;
	TSharedPtr<FUICommandInfo> ToggleShowLightningOverlay;
	TSharedPtr<FUICommandInfo> ToggleOnScreenProfiling;
	TSharedPtr<FUICommandInfo> ToggleShowCelestialDisplay;
	TSharedPtr<FUICommandInfo> ToggleShowCompositing;
	TSharedPtr<FUICommandInfo> ToggleShow3DCloudTextures;
	TSharedPtr<FUICommandInfo> ToggleShow2DCloudTextures;
	TSharedPtr<FUICommandInfo> TriggerRecompileShaders;
	TSharedPtr<FUICommandInfo> TriggerShowDocumentation;
	TSharedPtr<FUICommandInfo> TriggerExportCloudLayer;
	TSharedPtr<FUICommandInfo> TriggerCycleCompositingView;
};
#endif
FTrueSkyEditorPlugin* FTrueSkyEditorPlugin::Instance = NULL;

//TSharedRef<FTrueSkyEditorPlugin> staticSharedRef;
static std::string trueSkyPluginPath="../../Plugins/TrueSkyPlugin";
FTrueSkyEditorPlugin::FTrueSkyEditorPlugin()
	:cloudShadowRenderTarget(NULL)
	,haveEditor(false)
	,PathEnv(nullptr)
{
	InitPaths();
	DeployDefaultContent();
	Instance = this;
#ifdef SHARED_FROM_THIS
	//TSharedRef<FTrueSkyEditorPlugin> sharedRef=AsShared();
TSharedRef< FTrueSkyEditorPlugin,(ESPMode::Type)0 > ref=AsShared();
#endif
#if UE_EDITOR
	EditorInstances.Reset();
#endif
	AutoSaveTimer = 0.0f;
	//we need to pass through real DeltaSecond; from our scene Actor?
	CachedDeltaSeconds = 0.0333f;
}

FTrueSkyEditorPlugin::~FTrueSkyEditorPlugin()
{
	Instance = nullptr;
}


bool FTrueSkyEditorPlugin::SupportsDynamicReloading()
{
	return false;
}

void FTrueSkyEditorPlugin::Tick( float DeltaTime )
{
	if(IsInGameThread())
	{

	}
	CachedDeltaSeconds = DeltaTime;
#ifdef ENABLE_AUTO_SAVING
#if UE_EDITOR
	if ( AutoSaveTimer > 0.0f )
	{
		if ( (AutoSaveTimer -= DeltaTime) <= 0.0f )
		{
			SaveAllEditorInstances();
			AutoSaveTimer = 4.0f;
		}
	}
#endif
#endif
}

void FTrueSkyEditorPlugin::SetCloudShadowRenderTarget(FRenderTarget *t)
{
	cloudShadowRenderTarget=t;
}

#if UE_EDITOR
void FTrueSkyEditorPlugin::SetUIString(SEditorInstance* const EditorInstance,const char* name, const char*  value)
{
	if( StaticSetUIString != NULL&&EditorInstance!=NULL)
	{
		StaticSetUIString(EditorInstance->EditorWindowHWND,name, value );
	}
	else
	{
		UE_LOG(TrueSky, Warning, TEXT("Trying to set UI string before StaticSetUIString has been set"), TEXT(""));
	}
}

#endif
/** Tickable object interface */
void FTrueSkyTickable::Tick( float DeltaTime )
{
	if(FTrueSkyEditorPlugin::Instance)
		FTrueSkyEditorPlugin::Instance->Tick(DeltaTime);
}

bool FTrueSkyTickable::IsTickable() const
{
	return true;
}

TStatId FTrueSkyTickable::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTrueSkyTickable, STATGROUP_Tickables);
}

#define MAP_TOGGLE(U)	CommandList->MapAction( FTrueSkyCommands::Get().Toggle##U,\
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnToggle##U),\
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled),\
									FIsActionChecked::CreateRaw(this, &FTrueSkyEditorPlugin::IsToggled##U)\
									);

void FTrueSkyEditorPlugin::StartupModule()
{
#if UE_EDITOR
	FTrueSkyCommands::Register();
	if(FModuleManager::Get().IsModuleLoaded("MainFrame") )
	{
		haveEditor=true;
		IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
		const TSharedRef<FUICommandList>& CommandList = MainFrameModule.GetMainFrameCommandBindings();
		CommandList->MapAction( FTrueSkyCommands::Get().AddSequence,
								FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnAddSequence),
								FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsAddSequenceEnabled)
								);
			CommandList->MapAction( FTrueSkyCommands::Get().TriggerRecompileShaders,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnTriggerRecompileShaders),
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled)
									);
			CommandList->MapAction( FTrueSkyCommands::Get().TriggerShowDocumentation,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::ShowDocumentation)
									);
			CommandList->MapAction( FTrueSkyCommands::Get().TriggerExportCloudLayer,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::ExportCloudLayer)
									);
		{
			CommandList->MapAction( FTrueSkyCommands::Get().ToggleOnScreenProfiling,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnToggleOnScreenProfiling),
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled),
									FIsActionChecked::CreateRaw(this, &FTrueSkyEditorPlugin::IsToggledOnScreenProfiling)
									);
			CommandList->MapAction( FTrueSkyCommands::Get().ToggleShowCelestialDisplay,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnToggleShowCelestialDisplay),
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled),
									FIsActionChecked::CreateRaw(this, &FTrueSkyEditorPlugin::IsToggledShowCelestialDisplay)
									);
			MAP_TOGGLE(ShowFades)
			MAP_TOGGLE(ShowCubemaps)
			MAP_TOGGLE(ShowLightningOverlay)
			CommandList->MapAction( FTrueSkyCommands::Get().ToggleShowCompositing,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnToggleShowCompositing),
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled),
									FIsActionChecked::CreateRaw(this, &FTrueSkyEditorPlugin::IsToggledShowCompositing)
									);
			CommandList->MapAction( FTrueSkyCommands::Get().TriggerCycleCompositingView,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnTriggerCycleCompositingView),
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled)
									);
			CommandList->MapAction( FTrueSkyCommands::Get().ToggleShow3DCloudTextures,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnToggleShow3DCloudTextures),
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled),
									FIsActionChecked::CreateRaw(this, &FTrueSkyEditorPlugin::IsToggledShow3DCloudTextures)
									);
			CommandList->MapAction( FTrueSkyCommands::Get().ToggleShow2DCloudTextures,
									FExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::OnToggleShow2DCloudTextures),
									FCanExecuteAction::CreateRaw(this, &FTrueSkyEditorPlugin::IsMenuEnabled),
									FIsActionChecked::CreateRaw(this, &FTrueSkyEditorPlugin::IsToggledShow2DCloudTextures)
									);
		}

		MenuExtender = MakeShareable(new FExtender);
		MenuExtender->AddMenuExtension("WindowGlobalTabSpawners", EExtensionHook::After, CommandList, FMenuExtensionDelegate::CreateRaw(this, &FTrueSkyEditorPlugin::FillMenu));
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

		SequenceAssetTypeActions = MakeShareable(new FAssetTypeActions_TrueSkySequence);
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(SequenceAssetTypeActions.ToSharedRef());
	}
#endif
	
#if !UE_BUILD_SHIPPING && WITH_EDITOR
	// Register for debug drawing
	//UDebugDrawService::Register(TEXT("TrueSKY"), FDebugDrawDelegate::CreateUObject(this, &FTrueSkyEditorPlugin::OnDebugTrueSky));
#endif
	const FName IconName(TEXT("../../Plugins/TrueSkyPlugin/Resources/icon_64x.png"));

	OpenUI							=NULL;
	CloseUI							=NULL;
	SetView							=nullptr;
	PushStyleSheetPath				=NULL;
	SetSequence						=NULL;
	GetSequence						=NULL;
	SetOnPropertiesChangedCallback	=NULL;
	SetOnTimeChangedInUICallback	=NULL;
	SetTrueSkyUILogCallback			=NULL;
	SetGetColourTableCallback		=NULL;

	RenderingEnabled				=false;
	RendererInitialized				=false;
	StaticInitInterface				=NULL;
	StaticPushPath					=NULL;
	StaticOnDeviceChanged			=NULL;

	StaticTriggerAction				=NULL;
	StaticEnableUILogging			=NULL;
	PathEnv = NULL;
#if UE_EDITOR
	MessageId = RegisterWindowMessage(L"RESIZE");
#endif
}

void FTrueSkyEditorPlugin::OnDebugTrueSky(class UCanvas* Canvas, APlayerController*)
{
	const FColor OldDrawColor = Canvas->DrawColor;
	const FFontRenderInfo FontRenderInfo = Canvas->CreateFontRenderInfo(false, true);

	Canvas->SetDrawColor(FColor::White);

	UFont* RenderFont = GEngine->GetSmallFont();
	Canvas->DrawText(RenderFont, FString("trueSKY Debug Display"), 0.3f, 0.3f, 1.f, 1.f, FontRenderInfo);
	/*
		
	float res=Canvas->DrawText
	(
    UFont * InFont,
    const FString & InText,
    float X,
    float Y,
    float XScale,
    float YScale,
    const FFontRenderInfo & RenderInfo
)
		*/
	Canvas->SetDrawColor(OldDrawColor);
}

void FTrueSkyEditorPlugin::ShutdownModule()
{
#if !UE_BUILD_SHIPPING && WITH_EDITOR
	// Unregister for debug drawing
	//UDebugDrawService::Unregister(FDebugDrawDelegate::CreateUObject(this, &FTrueSkyEditorPlugin::OnDebugTrueSky));
#endif
	if(SetTrueSkyUILogCallback)
		SetTrueSkyUILogCallback(NULL);
	if(SetGetColourTableCallback)
		SetGetColourTableCallback(NULL);
#if UE_EDITOR
	FTrueSkyCommands::Unregister();
	if ( FModuleManager::Get().IsModuleLoaded("LevelEditor") )
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender( MenuExtender );

		FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(SequenceAssetTypeActions.ToSharedRef());
	}
#endif
	delete PathEnv;
	PathEnv = NULL;
}


#if UE_EDITOR
void FTrueSkyEditorPlugin::FillMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "TrueSky", FText::FromString(TEXT("TrueSky")) );
	{		
		MenuBuilder.AddMenuEntry( FTrueSkyCommands::Get().AddSequence );
		MenuBuilder.AddMenuEntry( FTrueSkyCommands::Get().TriggerRecompileShaders);
		MenuBuilder.AddMenuEntry( FTrueSkyCommands::Get().TriggerShowDocumentation);
		MenuBuilder.AddMenuEntry( FTrueSkyCommands::Get().TriggerExportCloudLayer);
		try
		{
			FNewMenuDelegate d;
#ifdef SHARED_FROM_THIS
			TSharedRef< FTrueSkyEditorPlugin,(ESPMode::Type)0 > ref=AsShared();
			d=FNewMenuDelegate::CreateSP(this, &FTrueSkyEditorPlugin::FillOverlayMenu);
			MenuBuilder.AddSubMenu(FText::FromString("Overlays"),FText::FromString("TrueSKY overlays"),d);
#else
			MenuBuilder.AddSubMenu(FText::FromString("Overlays"),FText::FromString("TrueSKY overlays"),FNewMenuDelegate::CreateRaw(this,&FTrueSkyEditorPlugin::FillOverlayMenu ));
#endif
		}
		catch(...)
		{
			UE_LOG(TrueSky, Warning, TEXT("Failed to add trueSKY submenu"), TEXT(""));
		}
	}
	MenuBuilder.EndSection();
	
}
	
void FTrueSkyEditorPlugin::FillOverlayMenu(FMenuBuilder& MenuBuilder)
{		
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleShowCelestialDisplay);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleOnScreenProfiling);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleShowFades);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleShowCubemaps);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleShowLightningOverlay);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleShowCompositing);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().TriggerCycleCompositingView);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleShow3DCloudTextures);
	MenuBuilder.AddMenuEntry(FTrueSkyCommands::Get().ToggleShow2DCloudTextures);
}
#endif
/** Returns environment variable value */
static FString GetEnvVariable( const wchar_t* const VariableName)
{
	wchar_t tmp[64];
	int iEnvSize=(int)GetEnvironmentVariableW(VariableName, tmp, 64)+1;
	FString res;
	if(iEnvSize>0)
	{
		wchar_t* Env=NULL;
		Env = new wchar_t[iEnvSize];
		check( Env );
		memset(Env, 0, iEnvSize * sizeof(wchar_t));
		GetEnvironmentVariableW(VariableName, Env, iEnvSize);
		if(Env)
			res=FString(Env);
		delete [] Env;
	}
	return res;
}

/** Takes Base path, concatenates it with Relative path */
static const TCHAR* ConstructPath(const TCHAR* const BasePath, const TCHAR* const RelativePath)
{
	if ( BasePath )
	{
		const int iPathLen = 1024;
		TCHAR* const NewPath = new TCHAR[iPathLen];
		check( NewPath );
		wcscpy_s( NewPath, iPathLen, BasePath );
		if ( RelativePath )
		{
			wcscat_s( NewPath, iPathLen, RelativePath );
		}
		return NewPath;
	}
	return NULL;
}

/** Takes Base path, concatenates it with Relative path and returns it as 8-bit char string */
static std::string ConstructPathUTF8(const TCHAR* const BasePath, const TCHAR* const RelativePath)
{
	if ( BasePath )
	{
		const int iPathLen = 1024;

		TCHAR* const NewPath = new TCHAR[iPathLen];
		check( NewPath );
		wcscpy_s( NewPath, iPathLen, BasePath );
		if ( RelativePath )
		{
			wcscat_s( NewPath, iPathLen, RelativePath );
		}

		char* const utf8NewPath = new char[iPathLen];
		check ( utf8NewPath );
		memset(utf8NewPath, 0, iPathLen);
		WideCharToMultiByte( CP_UTF8, 0, NewPath, iPathLen, utf8NewPath, iPathLen, NULL, NULL );

		delete [] NewPath;
		std::string ret=utf8NewPath;
		delete [] utf8NewPath;
		return ret;
	}
	return "";
}


/** Returns HWND for a given SWindow (if native!) */
#if UE_EDITOR
static HWND GetSWindowHWND(const TSharedPtr<SWindow>& Window)
{
	if ( Window.IsValid() )
	{
		TSharedPtr<FWindowsWindow> WindowsWindow = StaticCastSharedPtr<FWindowsWindow>(Window->GetNativeWindow());
		if ( WindowsWindow.IsValid() )
		{
			return WindowsWindow->GetHWnd();
		}
	}
	return 0;
}
#endif

static std::wstring Utf8ToWString(const char *src_utf8)
{
	int src_length=(int)strlen(src_utf8);
#ifdef _MSC_VER
	int length = MultiByteToWideChar(CP_UTF8, 0, src_utf8,src_length, 0, 0);
#else
	int length=src_length;
#endif
	wchar_t *output_buffer = new wchar_t [length+1];
#ifdef _MSC_VER
	MultiByteToWideChar(CP_UTF8, 0, src_utf8, src_length, output_buffer, length);
#else
	mbstowcs(output_buffer, src_utf8, (size_t)length );
#endif
	output_buffer[length]=0;
	std::wstring wstr=std::wstring(output_buffer);
	delete [] output_buffer;
	return wstr;
}
static std::string WStringToUtf8(const wchar_t *src_w)
{
	int src_length=(int)wcslen(src_w);
#ifdef _MSC_VER
	int size_needed = WideCharToMultiByte(CP_UTF8, 0,src_w, (int)src_length, NULL, 0, NULL, NULL);
#else
	int size_needed=2*src_length;
#endif
	char *output_buffer = new char [size_needed+1];
#ifdef _MSC_VER
	WideCharToMultiByte (CP_UTF8,0,src_w,(int)src_length,output_buffer, size_needed, NULL, NULL);
#else
	wcstombs(output_buffer, src_w, (size_t)size_needed );
#endif
	output_buffer[size_needed]=0;
	std::string str_utf8=std::string(output_buffer);
	delete [] output_buffer;
	return str_utf8;
}
static std::string FStringToUtf8(const FString &Source)
{
	const wchar_t *src_w = (const wchar_t*)(Source.GetCharArray().GetData());
	int src_length = (int)wcslen(src_w);
#ifdef _MSC_VER
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, src_w, (int)src_length, NULL, 0, NULL, NULL);
#else
	int size_needed = 2 * src_length;
#endif
	char *output_buffer = new char[size_needed + 1];
#ifdef _MSC_VER
	WideCharToMultiByte(CP_UTF8, 0, src_w, (int)src_length, output_buffer, size_needed, NULL, NULL);
#else
	wcstombs(output_buffer, src_w, (size_t)size_needed);
#endif
	output_buffer[size_needed] = 0;
	std::string str_utf8 = std::string(output_buffer);
	delete[] output_buffer;
	return str_utf8;
}
static std::string UE4ToSimulLanguageName(const FString &Source)
{
	std::string ue4_name = FStringToUtf8(Source);
	if (ue4_name.find("English") < ue4_name.size())
		return "en";
	if (ue4_name.find("Japanese") < ue4_name.size())
		return "ja";
	if (ue4_name.find("French") < ue4_name.size())
		return "fr";
	if (ue4_name.find("German") < ue4_name.size())
		return "de";
	if (ue4_name.find("Korean") < ue4_name.size())
		return "ko";
	return "";
}

bool CheckDllFunction(void *fn,FString &str,const char *fnName)
{
	bool res=(fn!=NULL);
	if(!res)
	{
		if(!str.IsEmpty())
			str+=", ";
		str+=fnName;
	}
	return res;
}

typedef void* moduleHandle;

static moduleHandle GetDllHandle( const TCHAR* Filename )
{
	check(Filename);
	return ::LoadLibraryW(Filename);
}

#define GET_FUNCTION(fnName) fnName= (F##fnName)FPlatformProcess::GetDllExport(DllHandle, TEXT(#fnName) );
#define MISSING_FUNCTION(fnName) (!CheckDllFunction(fnName,failed_functions, #fnName))

#if UE_EDITOR
#define warnf(expr, ...)				{ if(!(expr)) FDebug::AssertFailed( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); CA_ASSUME(expr); }
FTrueSkyEditorPlugin::SEditorInstance* FTrueSkyEditorPlugin::CreateEditorInstance(   void* Env )
{
	static bool failed_once = false;
	FString DllPath(FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Simul/Win64")));
	FString DllFilename(FPaths::Combine(*DllPath, TEXT("TrueSkyUI_MD.dll")));
	if( !FPaths::FileExists(DllFilename) )
	{
		if (!failed_once)
			UE_LOG(TrueSky, Warning, TEXT("TrueSkyUI_MD DLL not found."));
		failed_once = true;
		return NULL;
	}
//	SetErrorMode(SEM_FAILCRITICALERRORS);
	void* DllHandle = FPlatformProcess::GetDllHandle((const TCHAR*)DllFilename.GetCharArray().GetData() );
	if(DllHandle==NULL)
	{
		DllHandle=GetDllHandle((const TCHAR*)DllFilename.GetCharArray().GetData());
		if(DllHandle==nullptr)
		{
			if (!failed_once)
				UE_LOG(TrueSky, Warning, TEXT("Failed to load %s for truesky ui."), (const TCHAR*)DllFilename.GetCharArray().GetData());
			failed_once = true;
			InitPaths();
			return NULL;
		}
	}
	if ( DllHandle != NULL )
	{
		GET_FUNCTION(OpenUI);
		GET_FUNCTION(CloseUI);

		GET_FUNCTION(SetView);
		GET_FUNCTION(StaticSetUIString);
		
		PushStyleSheetPath = (FPushStyleSheetPath)FPlatformProcess::GetDllExport(DllHandle, TEXT("PushStyleSheetPath") );
		SetSequence = (FSetSequence)FPlatformProcess::GetDllExport(DllHandle, TEXT("StaticSetSequence") );
		GetSequence = (FGetSequence)FPlatformProcess::GetDllExport(DllHandle, TEXT("StaticGetSequence") );
		SetOnPropertiesChangedCallback = (FSetOnPropertiesChangedCallback)FPlatformProcess::GetDllExport(DllHandle, TEXT("SetOnPropertiesChangedCallback") );
		SetOnTimeChangedInUICallback = (FSetOnTimeChangedInUICallback)FPlatformProcess::GetDllExport(DllHandle, TEXT("SetOnTimeChangedCallback") );
		GET_FUNCTION(SetTrueSkyUILogCallback);
		GET_FUNCTION(StaticEnableUILogging);
		GET_FUNCTION(SetGetColourTableCallback)
		FString failed_functions;
		int num_fails=MISSING_FUNCTION(OpenUI) +MISSING_FUNCTION(CloseUI)
			+MISSING_FUNCTION(StaticSetUIString)
			+MISSING_FUNCTION(PushStyleSheetPath)
			+MISSING_FUNCTION(SetSequence)
			+MISSING_FUNCTION(GetSequence)
			+MISSING_FUNCTION(SetOnPropertiesChangedCallback)
			+MISSING_FUNCTION(SetOnTimeChangedInUICallback)
			+MISSING_FUNCTION(SetTrueSkyUILogCallback)
			+MISSING_FUNCTION(SetGetColourTableCallback);
		if(num_fails>0)
		{
			static bool reported=false;
			if(!reported)
			{
				UE_LOG(TrueSky, Error
					,TEXT("Can't initialize the trueSKY UI plugin dll because %d functions were not found - please update TrueSkyUI_MD.dll or TrueSkyEditorPlugin.cpp.\nThe missing functions are %s.")
					,num_fails
					,*failed_functions
					);
				reported=true;
			}
			return NULL;
		}
		checkf( OpenUI, L"OpenUI function not found!" );
		checkf( CloseUI, L"CloseUI function not found!" );
		checkf( PushStyleSheetPath, L"PushStyleSheetPath function not found!" );
		checkf( SetSequence, L"SetSequence function not found!" );
		checkf( GetSequence, L"GetSequence function not found!" );
		checkf( SetOnPropertiesChangedCallback, L"SetOnPropertiesChangedCallback function not found!" );
		checkf( SetOnTimeChangedInUICallback, L"SetOnTimeChangedInUICallback function not found!" );
		checkf( SetTrueSkyUILogCallback, L"SetTrueSkyUILogCallback function not found!" );
		
		checkf( StaticSetUIString, L"StaticSetUIString function not found!" );

		PushStyleSheetPath((trueSkyPluginPath+"\\Resources\\qss\\").c_str());

		IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
		TSharedPtr<SWindow> ParentWindow = MainFrameModule.GetParentWindow();
		if ( ParentWindow.IsValid() )
		{
			SEditorInstance EditorInstance;
			memset(&EditorInstance, 0, sizeof(EditorInstance));

			TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab);

			EditorInstance.EditorWindow = SNew(SWindow)
				.Title( FText::FromString(TEXT("TrueSky")) )
				.ClientSize( FVector2D(800.0f, 600.0f) )
				.AutoCenter( EAutoCenter::PrimaryWorkArea )
				.SizingRule( ESizingRule::UserSized )
				// .IsPopupWindow( true );
				;
			EditorInstance.EditorWindow->SetOnWindowClosed( FOnWindowClosed::CreateRaw(this, &FTrueSkyEditorPlugin::OnMainWindowClosed) );
			FSlateApplication::Get().AddWindowAsNativeChild( EditorInstance.EditorWindow.ToSharedRef(), ParentWindow.ToSharedRef() );

			EditorInstance.EditorWindowHWND = GetSWindowHWND(EditorInstance.EditorWindow);
			if ( EditorInstance.EditorWindowHWND )
			{
				const LONG_PTR wndStyle = GetWindowLongPtr( EditorInstance.EditorWindowHWND, GWL_STYLE );
				SetWindowLongPtr( EditorInstance.EditorWindowHWND, GWL_STYLE, wndStyle | WS_CLIPCHILDREN );

				const FVector2D ClientSize = EditorInstance.EditorWindow->GetClientSizeInScreen();
				const FMargin Margin = EditorInstance.EditorWindow->GetWindowBorderSize();
				
			//	GetWindowRect(EditorInstance.EditorWindowHWND, &ParentRect);
				EditorInstance.EditorWindow->Restore();
				FSlateRect winRect=EditorInstance.EditorWindow->GetClientRectInScreen();
				RECT ParentRect;
				ParentRect.left = 0;
				ParentRect.top = 0;
				ParentRect.right = winRect.Right-winRect.Left;
				ParentRect.bottom = winRect.Bottom-winRect.Top;
				RECT ClientRect;
				ClientRect.left = Margin.Left;
				ClientRect.top = Margin.Top + EditorInstance.EditorWindow->GetTitleBarSize().Get();
				ClientRect.right = ClientSize.X - Margin.Right;
				ClientRect.bottom = ClientSize.Y - Margin.Bottom;
				
				// Let's do this FIRST in case of errors in OpenUI!
				SetTrueSkyUILogCallback( TrueSkyUILogCallback );

				OpenUI( EditorInstance.EditorWindowHWND, &ClientRect, &ParentRect, Env, UNREAL_STYLE,"ue4");

				FInternationalization& I18N = FInternationalization::Get();
				FCultureRef culture = I18N.GetCurrentCulture();
				FString cult_name=culture->GetEnglishName();
				StaticSetUIString(EditorInstance.EditorWindowHWND, "Language", UE4ToSimulLanguageName(cult_name).c_str());

				// Overload main window's WndProc
				EditorInstance.OrigEditorWindowWndProc = (WNDPROC)GetWindowLongPtr( EditorInstance.EditorWindowHWND, GWLP_WNDPROC );
				SetWindowLongPtr( EditorInstance.EditorWindowHWND, GWLP_WNDPROC, (LONG_PTR)EditorWindowWndProc );

				// Setup notification callback
				SetOnPropertiesChangedCallback(  OnSequenceChangeCallback );
				SetGetColourTableCallback(GetColourTableCallback);
				SetOnTimeChangedInUICallback(OnTimeChangedCallback);
				EditorInstance.EditorWindow->Restore();
				return &EditorInstances[ EditorInstances.Add(EditorInstance) ];
			}
		}
	}

	return NULL;
}


FTrueSkyEditorPlugin::SEditorInstance* FTrueSkyEditorPlugin::FindEditorInstance(const TSharedRef<SWindow>& EditorWindow)
{
	for (int i = 0; i < EditorInstances.Num(); ++i)
	{
		if ( EditorInstances[i].EditorWindow.ToSharedRef() == EditorWindow )
		{
			return &EditorInstances[i];
		}
	}
	return NULL;
}

FTrueSkyEditorPlugin::SEditorInstance* FTrueSkyEditorPlugin::FindEditorInstance(HWND const EditorWindowHWND)
{
	for (int i = 0; i < EditorInstances.Num(); ++i)
	{
		if ( EditorInstances[i].EditorWindowHWND == EditorWindowHWND )
		{
			return &EditorInstances[i];
		}
	}
	return NULL;
}

FTrueSkyEditorPlugin::SEditorInstance* FTrueSkyEditorPlugin::FindEditorInstance(UTrueSkySequenceAsset* const Asset)
{
	for (int i = 0; i < EditorInstances.Num(); ++i)
	{
		if ( EditorInstances[i].Asset == Asset )
		{
			return &EditorInstances[i];
		}
	}
	return NULL;
}

int FTrueSkyEditorPlugin::FindEditorInstance(FTrueSkyEditorPlugin::SEditorInstance* const Instance)
{
	for (int i = 0; i < EditorInstances.Num(); ++i)
	{
		if ( &EditorInstances[i] == Instance )
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void FTrueSkyEditorPlugin::SaveAllEditorInstances()
{
	for (int i = 0; i < EditorInstances.Num(); ++i)
	{
		EditorInstances[i].SaveSequenceData();
	}
}

void FTrueSkyEditorPlugin::SEditorInstance::SaveSequenceData()
{
	if ( Asset.IsValid()&&Asset->IsValidLowLevel())
	{
		struct Local
		{
			static char* AllocString(int size)
			{
				check( size > 0 );
				return new char[size];
			}
		};
		check( FTrueSkyEditorPlugin::Instance );
		check( FTrueSkyEditorPlugin::Instance->GetSequence );
		if ( char* const OutputText = FTrueSkyEditorPlugin::Instance->GetSequence(EditorWindowHWND, Local::AllocString) )
		{
			if ( Asset->SequenceText.Num() > 0 )
			{
				if ( strcmp(OutputText, (const char*)Asset->SequenceText.GetData()) == 0 )
				{
					// No change -> quit
					delete OutputText;
					return;
				}
			}

			const int OutputTextLen = strlen( OutputText );
			Asset->SequenceText.Reset( OutputTextLen + 1 );

			for (char* ptr = OutputText; *ptr; ++ptr)
			{
				Asset->SequenceText.Add( *ptr );
			}
			Asset->SequenceText.Add( 0 );

			// Mark as dirty
			Asset->Modify( true );
			
			delete OutputText;
		}
	}
	else
	{
	}
}

void FTrueSkyEditorPlugin::SEditorInstance::LoadSequenceData()
{
	if ( Asset.IsValid()&&Asset->IsValidLowLevel() && Asset->SequenceText.Num() > 0 )
	{
		check( FTrueSkyEditorPlugin::Instance );
		check( FTrueSkyEditorPlugin::Instance->SetSequence );
		FTrueSkyEditorPlugin::Instance->SetSequence( EditorWindowHWND, (const char*)Asset->SequenceText.GetData(),Asset->SequenceText.Num());
	}
	else
	{
	}
}


void FTrueSkyEditorPlugin::OnMainWindowClosed(const TSharedRef<SWindow>& Window)
{
	if ( SEditorInstance* const EditorInstance = FindEditorInstance(Window) )
	{
		EditorInstance->SaveSequenceData();

		check( CloseUI );
		CloseUI( EditorInstance->EditorWindowHWND );

		EditorInstance->EditorWindow = NULL;
		EditorInstances.RemoveAt( FindEditorInstance(EditorInstance) );
	}
}


/** Called when TrueSkyUI properties have changed */
void FTrueSkyEditorPlugin::OnSequenceChangeCallback(HWND OwnerHWND,const char *txt)
{
	check( Instance );
	if ( SEditorInstance* const EditorInstance = Instance->FindEditorInstance(OwnerHWND) )
	{
		if (EditorInstance->Asset.IsValid()&&EditorInstance->Asset->IsValidLowLevel())
			EditorInstance->SaveSequenceData();
		else
		{
			// Don't do this in a callback!
			////if(FTrueSkyEditorPlugin::Instance&&FTrueSkyEditorPlugin::Instance->CloseUI)
			//	FTrueSkyEditorPlugin::Instance->CloseUI(EditorInstance->EditorWindowHWND);
		}
		ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
		if (trueSkyPlugin.GetActiveSequence() == EditorInstance->Asset)
		{
			trueSkyPlugin.OnUIChangedSequence();
		}
	}
}

void FTrueSkyEditorPlugin::TrueSkyUILogCallback(const char *txt)
{
	if(!Instance )
		return;
	static FString fstr;
	fstr+=txt;
	int max_len=0;
	for(int i=0;i<fstr.Len();i++)
	{
		if(fstr[i]==L'\n'||i>1000)
		{
			fstr[i]=L' ';
			max_len=i+1;
			break;
		}
	}
	if(max_len==0)
		return;
	FString substr=fstr.Left(max_len);
	fstr=fstr.RightChop(max_len);
	if(substr.Contains("error"))
	{
		UE_LOG(TrueSky,Error,TEXT("%s"), *substr);
	}
	else if(substr.Contains("warning"))
	{
		UE_LOG(TrueSky,Warning,TEXT("%s"), *substr);
	}
	else
	{
		UE_LOG(TrueSky,Display,TEXT("%s"), *substr);
	}
}

static void editorCallback()
{
	//FTrueSkyEditorPlugin::Instance->RetrieveColourTables();
}

void FTrueSkyEditorPlugin::RetrieveColourTables()
{
}

bool FTrueSkyEditorPlugin::GetColourTableCallback(unsigned kf_uid,int x,int y,int z,float *target)
{
	ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
	trueSkyPlugin.RequestColourTable(kf_uid,x,y,z);
	if(target)
	{
		const ITrueSkyPlugin::ColourTableRequest *req=nullptr;

		int count=0;

		while((req=trueSkyPlugin.GetColourTable(kf_uid))==nullptr&&count<10)
		{
			Sleep(10);
			count++;
		}

		if(req&&req->valid&&req->data)
		{
			memcpy(target,req->data,x*y*z*4*sizeof(float));
			trueSkyPlugin.ClearColourTableRequests();
			return true;
		}
	}
	return false;
}

void FTrueSkyEditorPlugin::OnTimeChangedCallback(HWND OwnerHWND,float t)
{
	check( Instance );
	if ( SEditorInstance* const EditorInstance = Instance->FindEditorInstance(OwnerHWND) )
	{
		//EditorInstance->SaveSequenceData();
	}
	check( Instance );
	if ( SEditorInstance* const EditorInstance = Instance->FindEditorInstance(OwnerHWND) )
	{
		//EditorInstance->SaveSequenceData();
		ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
		if (trueSkyPlugin.GetActiveSequence() == EditorInstance->Asset)
		{
			trueSkyPlugin.OnUIChangedTime(t);
		}
	}
}

::UINT FTrueSkyEditorPlugin::MessageId = 0;

LRESULT CALLBACK FTrueSkyEditorPlugin::EditorWindowWndProc(HWND hWnd, ::UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if ( uMsg == WM_SIZE)//||uMsg==WM_ACTIVATE||uMsg==WM_MOVE)
	{
		if ( HWND ChildHWND = GetWindow(hWnd, GW_CHILD) ) 
		{
			PostMessage(ChildHWND, MessageId, wParam, lParam);
		}
	}
	check( Instance );
	if(Instance&&Instance->SetView)
	{
		ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
		const TArray <FLevelEditorViewportClient *>  &viewports=GEditor->LevelViewportClients;//LevelEditorViewportClients;//;
		for(int i=0;i<viewports.Num();i++)
		{
			FEditorViewportClient *viewport=viewports[i];
			const FViewportCameraTransform &trans=viewport->GetViewTransform();
			
			FVector pos		=trans.GetLocation();
			FRotator rot	=trans.GetRotation();//.GetInverse();
			
			static float U=0.f,V=90.f,W=270.f;
			FRotator r2(U,V,W);
			FRotationMatrix R2(r2);

			FTransform tra(rot,pos);
		
			FMatrix viewmat		=tra.ToMatrixWithScale();
			FMatrix u			=R2.operator*(viewmat);
			FMatrix viewMatrix	=u.Inverse();
			// This gives us a matrix that's "sort-of" like the view matrix, but not.
			// So we have to mix up the y and z
			FMatrix proj;
			proj.SetIdentity();
			trueSkyPlugin.AdaptViewMatrix(viewMatrix);
			Instance->SetView(i,(const float*)&viewMatrix,(const float *)&proj);
		}
	}
	if(SEditorInstance* const EditorInstance = Instance->FindEditorInstance(hWnd))
	{
		return CallWindowProc( EditorInstance->OrigEditorWindowWndProc, hWnd, uMsg, wParam, lParam );
	}
	return 0;
}
#endif

void FTrueSkyEditorPlugin::InitPaths()
{
	if ( PathEnv == NULL )
	{
		static FString path;
		path= GetEnvVariable(L"PATH");
		FString DllPath(FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Simul/Win64")));
			
		path=(DllPath+L";")+path;
		PathEnv=(const TCHAR*)path.GetCharArray().GetData() ;
		SetEnvironmentVariable( L"PATH", PathEnv);
	}
}

void FTrueSkyEditorPlugin::OnToggleRendering()
{
	ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
	trueSkyPlugin.OnToggleRendering();
}

IMPLEMENT_TOGGLE(ShowFades)
IMPLEMENT_TOGGLE(ShowCubemaps)
IMPLEMENT_TOGGLE(ShowLightningOverlay)
IMPLEMENT_TOGGLE(ShowCompositing)
IMPLEMENT_TOGGLE(ShowCelestialDisplay)
IMPLEMENT_TOGGLE(OnScreenProfiling)
IMPLEMENT_TOGGLE(Show3DCloudTextures)
IMPLEMENT_TOGGLE(Show2DCloudTextures)

bool FTrueSkyEditorPlugin::IsToggleRenderingEnabled()
{
	ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
	if(trueSkyPlugin.GetActiveSequence())
	{
		return true;
	}
	// No active sequence found!
	trueSkyPlugin.SetRenderingEnabled(false);
	return false;
}

bool FTrueSkyEditorPlugin::IsMenuEnabled()
{
	return true;
}

bool FTrueSkyEditorPlugin::IsToggleRenderingChecked()
{
	return RenderingEnabled;
}

void FTrueSkyEditorPlugin::DeployDefaultContent()
{
	// put the default stuff in:
	FString EnginePath		=FPaths::EngineDir();
	FString ProjectPath		=FPaths::GetPath(FPaths::GetProjectFilePath());
	FString ProjectContent	=FPaths::Combine(*ProjectPath,TEXT("Content"));
	FString SrcSimulContent	=FPaths::Combine(*EnginePath, TEXT("Plugins/TrueSkyPlugin/DeployToContent"));
	FString DstSimulContent	=FPaths::Combine(*ProjectContent, TEXT("TrueSky"));
	if(FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*SrcSimulContent))
	{
		FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*DstSimulContent, *SrcSimulContent, true);
	}
}

void FTrueSkyEditorPlugin::OnAddSequence()
{
	ULevel* const Level = GWorld->PersistentLevel;
	ATrueSkySequenceActor* SequenceActor = NULL;
	// Check for existing sequence actor
	for(int i = 0; i < Level->Actors.Num() && SequenceActor == NULL; i++)
	{
		SequenceActor = Cast<ATrueSkySequenceActor>( Level->Actors[i] );
	}
	if ( SequenceActor == NULL )
	{
		// Add sequence actor
		SequenceActor=GWorld->SpawnActor<ATrueSkySequenceActor>(ATrueSkySequenceActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	}
	else
	{
		// Sequence actor already exists -- error message?
	}
	DeployDefaultContent();
	if(SequenceActor)
	{
		ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
		trueSkyPlugin.InitializeDefaults(SequenceActor);
	}
}

#include "DesktopPlatformModule.h"

void FTrueSkyEditorPlugin::ExportCloudLayer()
{
	FString filename;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
	TSharedPtr<SWindow> ParentWindow = MainFrameModule.GetParentWindow();
	TArray<FString> OutFilenames;
	if (DesktopPlatform)
	if ( ParentWindow.IsValid() )
	{
		void* ParentWindowWindowHandle = NULL;
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}
	FString InOutLastPath = FPaths::GameDir() ;
	
		FString FileTypes = TEXT("Fbx File (*.fbx)|*.fbx|All Files (*.*)|*.*");
		DesktopPlatform->SaveFileDialog(
			ParentWindowWindowHandle,
			FString("Save to FBX"),
			InOutLastPath,
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OutFilenames
		);
		if(OutFilenames.Num())
			filename=OutFilenames[0];
	}
	ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();
	trueSkyPlugin.ExportCloudLayer(filename,0);
}

void FTrueSkyEditorPlugin::OnSequenceDestroyed()
{
}

bool FTrueSkyEditorPlugin::IsAddSequenceEnabled()
{
	// No longer returns false if TrueSkySequenceActor already exists. Because multiple actors is ok.
/*	ULevel* const Level = GWorld->PersistentLevel;
	for(int i=0;i<Level->Actors.Num();i++)
	{
		if ( Cast<ATrueSkySequenceActor>(Level->Actors[i]) )
			return false;
	}*/
	return true;
}

void FTrueSkyEditorPlugin::OpenEditor(UTrueSkySequenceAsset* const TrueSkySequence)
{
#if UE_EDITOR
	if ( TrueSkySequence == NULL )
		return;

	SEditorInstance* EditorInstance = FindEditorInstance(TrueSkySequence);
	if ( EditorInstance == NULL )
	{
		
		ITrueSkyPlugin &trueSkyPlugin=ITrueSkyPlugin::Get();

//		trueSkyPlugin.InitRenderingInterface();
 		//void* const Env = (trueSkyPlugin.GetActiveSequence() == TrueSkySequence)? trueSkyPlugin.GetRenderEnvironment() : NULL;

		EditorInstance = CreateEditorInstance(  NULL );
		if ( EditorInstance )
		{
			EditorInstance->Asset = TrueSkySequence;
			EditorInstance->EditorWindow->SetTitle(FText::FromString(TrueSkySequence->GetName()));
#ifdef ENABLE_AUTO_SAVING
			AutoSaveTimer = 4.0f;
#endif
		}
	}
	else
	{
		EditorInstance->EditorWindow->BringToFront();
	}
	// Set sequence asset to UI
	if ( EditorInstance )
	{
		EditorInstance->LoadSequenceData();
	}
#endif
}



