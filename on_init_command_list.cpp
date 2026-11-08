
///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  on_init_command_list : called once per game session, load textures
// ----------------------------------------------------------------------------------------
// 
// (c) Lefuneste.
//
// All rights reserved.
// https://github.com/xxx
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// This software is using part of code or algorithms provided by
// * Crosire https://github.com/crosire/reshade  
// * FransBouma https://github.com/FransBouma/ShaderToggler
// * ShortFuse https://github.com/clshortfuse/renodx
// 
/////////////////////////////////////////////////////////////////////////

#include <reshade.hpp>
#include <unordered_map>


#include "loader_addon_shared.h"
#include "addon_functions.h"
#include "addon_objects.h"
#include "addon_logs.h"

#include "to_string.hpp"


using namespace reshade::api;



#ifdef _DEBUG
extern "C" {
#endif

	//*******************************************************************************
	VREM_EXPORT void vrem_on_init_command_list(command_list* cmd_list) {

#if _DEBUG_LOGS
		// too verbose
		// log_init_command_list(cmd_list);
#endif

		cmd_list->create_private_data<CmdData>();

	}
#ifdef _DEBUG
}
#endif

#ifdef _DEBUG
extern "C" {
#endif

	//*******************************************************************************
	VREM_EXPORT void vrem_on_destroy_command_list(command_list* cmd_list) {

#if _DEBUG_LOGS
		// too verbose
		//log_destroy_command_list(cmd_list);
#endif

		cmd_list->destroy_private_data<CmdData>();

	}
#ifdef _DEBUG
}
#endif

#ifdef _DEBUG
extern "C" {
#endif

	//*******************************************************************************
	VREM_EXPORT void vrem_on_reset_command_list(command_list* cmd_list) {

#if _DEBUG_LOGS
		// too verbose
		//log_reset_command_list(cmd_list);
#endif
		auto& st = cmd_list->get_private_data<CmdData>();
		st = CmdData{};
	}
#ifdef _DEBUG
}
#endif
