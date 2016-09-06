// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TrueSkyPlugin : ModuleRules
	{
		public TrueSkyPlugin(TargetInfo Target)
		{
			//string trueSKYPath=UEBuildConfiguration.UEThirdPartySourceDirectory+"Vorbis/libvorbis-1.3.2/";

			if (Target.Configuration != UnrealTargetConfiguration.Debug)
			{
				Definitions.Add("NDEBUG=1");
			}

			PublicIncludePaths.AddRange(
					new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					}
				);
			if(UEBuildConfiguration.bBuildEditor==true)
			{
				PublicIncludePaths.AddRange(new string[] {
					"Editor/LevelEditor/Public",
					"Editor/PlacementMode/Private",
					"Editor/MainFrame/Public/Interfaces",
                    "Developer/AssetTools/Private",
				});
			}

			// ... Add private include paths required here ...
			PrivateIncludePaths.AddRange(
				new string[] {
					"TrueSkyPlugin/Private"
				}
				);

			// Add public dependencies that we statically link with here ...
			PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"Core",
						"CoreUObject",
						"Slate",
						"Engine",
					}
				);

			if(UEBuildConfiguration.bBuildEditor==true)
			{
				PublicDependencyModuleNames.AddRange(
						new string[]
						{
							"UnrealEd",
							"EditorStyle",
							"CollectionManager",
							"EditorStyle",
							"AssetTools",
							"PlacementMode",
							"ContentBrowser"
						}
					);
			}

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"RenderCore",
                    "RHI",
					"Slate",
					"SlateCore",
                    "Renderer"
					// ... add private dependencies that you statically link with here ...
				}
				);
			if(Target.Platform==UnrealTargetPlatform.Win64)
			{
				string SimulBinariesDir = "$(EngineDir)/Binaries/ThirdParty/Simul/Win64/";
				RuntimeDependencies.Add(new RuntimeDependency(SimulBinariesDir + "TrueSkyPluginRender_MT.dll"));
				PrivateDependencyModuleNames.Add("D3D11RHI");
				AddThirdPartyPrivateStaticDependencies(Target,"DX11"	);
			}
			if(Target.Platform==UnrealTargetPlatform.XboxOne)
			{
				PrivateIncludePathModuleNames.Add("XboxOneD3D11RHI");
				PrivateDependencyModuleNames.AddRange(
					new string[] {	"DX11",
									"XboxOneSDK"	}
					);
			}
			if(Target.Platform==UnrealTargetPlatform.PS4)
			{
				string SimulBinariesDir = "$(EngineDir)/Binaries/ThirdParty/Simul/PS4/";

				PrivateDependencyModuleNames.Add("PS4RHI");
				PublicIncludePaths.AddRange(new string[] {
					"Runtime/PS4/PS4RHI/Public"
					,"Runtime/PS4/PS4RHI/Private"
				});
                string trueskypluginrender = "trueskypluginrender";
                if (Target.Configuration == UnrealTargetConfiguration.Debug)
                {
                    trueskypluginrender += "-debug";
                }
                RuntimeDependencies.Add(new RuntimeDependency(SimulBinariesDir + trueskypluginrender+".prx"));
                PublicAdditionalLibraries.Add("../binaries/thirdparty/simul/ps4/"+trueskypluginrender+"_stub.a");
				PublicDelayLoadDLLs.AddRange(
					new string[] {
					trueskypluginrender+".prx"
				}
				);
                string SDKDir = System.Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
                if ((SDKDir == null) || (SDKDir.Length <= 0))
                {
                    SDKDir = "C:/Program Files (x86)/SCE/ORBIS SDKs/3.000";
                }
				PrivateIncludePaths.AddRange(
				    new string[] {
					    SDKDir+"/target/include_common/gnmx"
				    }
				);
			}

			PublicDelayLoadDLLs.Add("SimulBase_MD.dll");
			PublicDelayLoadDLLs.Add("SimulCamera_MD.dll");
			PublicDelayLoadDLLs.Add("SimulClouds_MD.dll");
			PublicDelayLoadDLLs.Add("SimulCrossPlatform_MD.dll");
			PublicDelayLoadDLLs.Add("SimulDirectX11_MD.dll");
			PublicDelayLoadDLLs.Add("SimulGeometry_MD.dll");
			PublicDelayLoadDLLs.Add("SimulMath_MD.dll");
			PublicDelayLoadDLLs.Add("SimulMeta_MD.dll");
			PublicDelayLoadDLLs.Add("SimulScene_MD.dll");
			PublicDelayLoadDLLs.Add("SimulSky_MD.dll");
			PublicDelayLoadDLLs.Add("SimulTerrain_MD.dll");
			PublicDelayLoadDLLs.Add("TrueSkyPluginRender_MT.dll");
		}
	}
}