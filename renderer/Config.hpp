/*
 * Copyright (C) 2020, Inria
 * GRAPHDECO research group, https://team.inria.fr/graphdeco
 * All rights reserved.
 *
 * This software is free for non-commercial, research and evaluation use 
 * under the terms of the LICENSE.md file.
 *
 * For inquiries contact sibr@inria.fr and/or George.Drettakis@inria.fr
 */


#pragma once

# include <core/system/Config.hpp>
# include <core/system/CommandLineArgs.hpp>

# ifdef SIBR_OS_WINDOWS
#  ifdef SIBR_STATIC_DEFINE
#    define SIBR_EXPORT
#    define SIBR_NO_EXPORT
#  else
#    ifndef SIBR_EXP_ULR_EXPORT
#      ifdef SIBR_EXP_ULR_EXPORTS
/* We are building this library */
#        define SIBR_EXP_ULR_EXPORT __declspec(dllexport)
#      else
/* We are using this library */
#        define SIBR_EXP_ULR_EXPORT __declspec(dllimport)
#      endif
#    endif
#    ifndef SIBR_NO_EXPORT
#      define SIBR_NO_EXPORT
#    endif
#  endif
# else
#  define SIBR_EXP_ULR_EXPORT
# endif

namespace sibr {

	/// Arguments for all ULR applications.
	struct GaussianAppArgs :
		virtual BasicIBRAppArgs {
		Arg<int> version = { "v", 3, "ULR implementation version" };
		ArgSwitch softVisibility = { "soft-visibility", false, "generate and use soft visibility masks" };
		Arg<bool> masks = { "masks" , "use binary masks" };
		Arg<std::string> snaplabel = { "snap-label" , "top" };
		Arg<std::string> maskParams = { "masks-param" , "" };
		Arg<std::string> maskParamsExtra = { "masks-param-extra" , "" };
		Arg<bool> invert = { "invert", "invert the masks" };
		Arg<bool> alphas = { "alphas", "" };
		Arg<std::string> modelPath = { "model-path", "" };
		Arg<std::string> scaffoldPath = { "scaffold", "" };
		Arg<bool> poisson = { "poisson-blend", "apply Poisson-filling to the ULR result" };
		Arg<int> budget = { "budget", 16000, "Hierarchy memory budget (MB)" };
		Arg<std::string> imagesPath = { "images-path", "", "path to images" };
		Arg<bool> tcpEnabled = {"tcpEnabled", "Enable camera controls on tcp socket 4444"};
	};

}

