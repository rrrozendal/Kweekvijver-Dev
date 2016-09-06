// Copyright 2015-2016 Simul Software Ltd All Rights Reserved.
#pragma once
#include "ITrueSkyPlugin.h"

// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.

#include "RenderResource.h"
#include "Engine.h"

namespace simul
{
	enum PluginStyle
	{
		DEFAULT_STYLE=0
		,UNREAL_STYLE=1
		,UNITY_STYLE=2
		, UNITY_STYLE_DEFERRED = 6
		, VISION_STYLE = 8
		, CUBEMAP_STYLE = 16
		, VR_STYLE = 32				// without right eye flag, this means left eye.
		, VR_STYLE_RIGHT_EYE = 64
		, POST_TRANSLUCENT = 128	// rain, snow etc.
	};
	inline PluginStyle operator|(PluginStyle a, PluginStyle b)
	{
		return static_cast<PluginStyle>(static_cast<int>(a) | static_cast<int>(b));
	}
	inline PluginStyle operator&(PluginStyle a, PluginStyle b)
	{
		return static_cast<PluginStyle>(static_cast<int>(a) & static_cast<int>(b));
	}
	inline PluginStyle operator~(PluginStyle a)
	{
		return static_cast<PluginStyle>(~static_cast<int>(a));
	}

	enum GraphicsDeviceType
	{
		GraphicsDeviceUnknown=-1,
		GraphicsDeviceOpenGL = 0,          // OpenGL
		GraphicsDeviceD3D9,                // Direct3D 9
		GraphicsDeviceD3D11,               // Direct3D 11
		GraphicsDeviceGCM,                 // Sony PlayStation 3 GCM
		GraphicsDeviceNull,                // "null" device (used in batch mode)
		GraphicsDeviceHollywood,           // Nintendo Wii
		GraphicsDeviceXenon,               // Xbox 360
		GraphicsDeviceOpenGLES,            // OpenGL ES 1.1
		GraphicsDeviceOpenGLES20Mobile,    // OpenGL ES 2.0 mobile variant
		GraphicsDeviceMolehill,            // Flash 11 Stage3D
		GraphicsDeviceOpenGLES20Desktop,   // OpenGL ES 2.0 desktop variant (i.e. NaCl)
		GraphicsDevicePS4,
		GraphicsDeviceXboxOne,
		GraphicsDeviceCount
	};


	/// Event types for UnitySetGraphicsDevice
	enum GraphicsEventType
	{
		GraphicsEventInitialize = 0,
		GraphicsEventShutdown,
		GraphicsEventBeforeReset,
		GraphicsEventAfterReset,
	};

	struct Viewport
	{
		int x,y,w,h;

	};

	struct LightningBolt
	{
		int id;
		float pos[3];
		float endpos[3];
		float brightness;
		float colour[3];
		int age;
	};
}