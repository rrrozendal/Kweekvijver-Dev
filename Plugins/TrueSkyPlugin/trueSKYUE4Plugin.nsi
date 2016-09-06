!include "MUI.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; The name of the installer
Name "Simul trueSKY UE4 Plugin"
!ifndef SIMUL_VERSION
!define SIMUL_VERSION '4.0.11.0026'
!endif
!ifndef UE_VERSION
!define UE_VERSION '4.11'
!endif
!ifndef SIMUL
!define SIMUL C:\Simul\master\Simul
!endif
!ifdef SUPPORT_PS4
	!ifdef SUPPORT_XBOXONE
		!define PLAT _x64_PS4_XboxOne
	!else
		!define PLAT _x64_PS4
	!endif
!else
	!ifdef SUPPORT_XBOXONE
		!define PLAT _x64_XboxOne
	!else
		!define PLAT _x64
	!endif
!endif
!ifndef OUTPUT_FILE
!define OUTPUT_FILE 'trueSKYUE4Plugin_${UE_VERSION}_${SIMUL_VERSION}${PLAT}.exe'
!endif
!ifndef DEFAULT_UE_DIRECTORY
!define DEFAULT_UE_DIRECTORY 'C:\Program Files\Unreal Engine'
!endif
; The file to write
; ${StrReplace} "${OUTPUT_FILE}" "\" "//"
OutFile "${OUTPUT_FILE}"
Icon "Resources\Simul Icon.ico"

; The default installation directory
InstallDir "${DEFAULT_UE_DIRECTORY}\"
RequestExecutionLevel admin

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "Resources\Simul Logo 150x57.bmp"

!define MUI_ABORTWARNING

!define WELCOME_TITLE 'Simul trueSKY UE4 Plugin ${SIMUL_VERSION}'
!define MUI_ICON "Resources\Simul Icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "Resources\Simul Logo 164x314.bmp" 
!define MUI_WELCOMEPAGE_TITLE '${WELCOME_TITLE}'
!define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH
!define MUI_WELCOMEPAGE_TEXT "This wizard will install required files for the Simul trueSKY UE4 Plugin.\n\nClick Next to continue."
!define MUI_DIRECTORYPAGE_TEXT_TOP "Please identify the UE4 directory."
!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE "DirectoryLeave"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Function DirectoryLeave
IfFileExists "$INSTDIR\Engine" EngineExists EngineNotThere
EngineNotThere:
MessageBox MB_OK "$INSTDIR doesn't seem to contain an Unreal Engine installation"
Abort
EngineExists:
FunctionEnd

!macro AddFiles Targ Src
	SetOutPath ${Targ}
	!ifdef SUPPORT_PS4
		!ifdef SUPPORT_XBOXONE
			File /r /x "*.exe" /x "*.obj" ${Src}
		!else
			File /r /x XboxOne /x "*.exe" /x "*.obj" ${Src}
		!endif
	!else
		!ifdef SUPPORT_XBOXONE
			File /r /x PS4 /x "*.exe" /x "*.obj" ${Src}
		!else
			File /r /x XboxOne /x PS4 /x "*.exe" /x "*.obj" ${Src}
		!endif
	!endif
!macroend

Section "-all"
	SetOutPath "$INSTDIR\Engine\Binaries\ThirdParty\Simul\Win64\plugins\platforms"
	File "..\..\Binaries\ThirdParty\Simul\Win64\plugins\platforms\qminimal.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\plugins\platforms\qwindows.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\plugins\bearer\qgenericbearer.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\plugins\bearer\qnativewifibearer.dll"

	SetOutPath "$INSTDIR\Engine\Binaries\ThirdParty\Simul\Win64"
	File "..\..\Binaries\ThirdParty\Simul\Win64\D3Dcompiler_46.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5CLucene.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5Core.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5Gui.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5Help.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5Network.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5OpenGL.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5Sql.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5Widgets.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\Qt5Xml.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SequencerQtWidgets_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulBase_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulClouds_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulCrossPlatform_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulGeometry_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulMath_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulQtWidgets_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulScene_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\SimulSky_MD.dll"
	File "..\..\Binaries\ThirdParty\Simul\Win64\TrueSkyUI_MD.dll"
	
	File "..\..\Binaries\ThirdParty\Simul\Win64\TrueSkyPluginRender_MT.dll"

	!ifdef SUPPORT_PS4
		SetOutPath "$INSTDIR\Engine\Binaries\ThirdParty\Simul\PS4"
		File "..\..\Binaries\ThirdParty\Simul\PS4\TrueSkyPluginRender.prx"
		File "..\..\Binaries\ThirdParty\Simul\PS4\TrueSkyPluginRender_stub.a"
		File "..\..\Binaries\ThirdParty\Simul\PS4\TrueSkyPluginRender_stub_weak.a"
		IfFileExists "$INSTDIR\Engine\Source\Programs\AutomationTool\PS4\PS4Platform.Automation.cs" 0 copy_ps4
		MessageBox MB_YESNO "PS4Platform.Automation.cs already exists, but we have a modified version that deploys the trueSKY files.$\r$\nPlease choose whether to overwrite the file.$\r$\nIf not, the modified file will be placed in Engine\Plugins\TrueSkyPlugin and you can merge in the changes.$\r$\nShould the installer overwrite the existing file?" /SD IDYES IDNO no_copy_ps4 
		copy_ps4:
			SetOutPath "$INSTDIR\Engine\Source\Programs\AutomationTool\PS4"
			File "..\..\Source\Programs\AutomationTool\PS4\PS4Platform.Automation.cs"
			Goto next_ps4
		no_copy_ps4:
			SetOutPath "$INSTDIR\Engine\Plugins\TrueSkyPlugin"
			File "..\..\Source\Programs\AutomationTool\PS4\PS4Platform.Automation.cs"
		next_ps4:
	!endif
	!ifdef SUPPORT_XBOXONE
		SetOutPath "$INSTDIR\Engine\Binaries\ThirdParty\Simul\XboxOne"
		File "..\..\Binaries\ThirdParty\Simul\XboxOne\TrueSkyPluginRender_MD.dll"
		File "..\..\Binaries\ThirdParty\Simul\XboxOne\TrueSkyPluginRender_MD.pdb"
		IfFileExists "$INSTDIR\Engine\Source\Programs\AutomationTool\XboxOne\XboxOnePlatform.Automation.cs" 0 copy_xboxone
		MessageBox MB_YESNO "XboxOnePlatform.Automation.cs already exists, but we have a modified version that deploys the trueSKY files.$\r$\nPlease choose whether to overwrite the file.$\r$\nIf not, the modified file will be placed in Engine\Plugins\TrueSkyPlugin and you can merge in the changes.$\r$\nShould the installer overwrite the existing file?" /SD IDYES IDNO no_copy_xboxone 
		copy_xboxone:
			SetOutPath "$INSTDIR\Engine\Source\Programs\AutomationTool\XboxOne"
			File "..\..\Source\Programs\AutomationTool\XboxOne\XboxOnePlatform.Automation.cs"
			Goto next_xboxone
		no_copy_xboxone:
			SetOutPath "$INSTDIR\Engine\Plugins\TrueSkyPlugin"
			File "..\..\Source\Programs\AutomationTool\XboxOne\XboxOnePlatform.Automation.cs"
		next_xboxone:
	!endif
	SetOutPath "$INSTDIR\Engine\Plugins\TrueSkyPlugin"
	!insertmacro AddFiles "$INSTDIR\Engine\Plugins\TrueSkyPlugin" "*.*"
	!insertmacro AddFiles "$INSTDIR\Engine\Binaries\ThirdParty\Simul" "..\..\Binaries\ThirdParty\Simul\*.*"

	; /x *.pdb would result in smaller installer."
	SetOutPath "$INSTDIR\Engine\Plugins\TrueSkyPlugin\Modified"

	File "..\..\Source\Runtime\Renderer\Public\RendererInterface.h"
	File "..\..\Source\Runtime\Renderer\Private\SceneRendering.cpp"
SectionEnd

Function .onInstSuccess
	IfSilent +2 0
		ExecShell "open" "http://docs.simul.co/unrealengine/Deploy.html"
FunctionEnd
