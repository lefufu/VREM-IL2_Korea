
///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  process_pipeline_hash : do all action associated to shaders
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


// *******************************************************************************************************
// process shader when pipeline is bound, to replace shader
//
bool process_pipeline_hash_bind(CmdData cmddata, FilteredPipelineData pipe_data, command_list* commandList, pipeline_stage stages, pipeline currentPipeline)
{
	// true if an action for draw is associated with at least one shader of the pipeline
	bool return_value = false;
	bool to_replace = false;

	// check all shaders to replace by the clone if and set the flag for draw if needed
	for (const auto& shader : pipe_data.shader_definitions)
	{
		if (shader.action & action_replace_bind)
		{
			to_replace = true;
		}

		// all actions need processing in on_draw()
		if (shader.action != action_null)
		{
			return_value = true;
		}
	}

	// replace the pipeline if needed
	if (to_replace && pipe_data.cloned_pipeline.handle != 0)
	{
		
		commandList->bind_pipeline(stages, pipe_data.cloned_pipeline);
		// log infos
#if _DEBUG_LOGS  
		log_pipeline_replaced(currentPipeline.handle, pipe_data.cloned_pipeline.handle);
#endif
	}

	return return_value;
}

// *******************************************************************************************************
// process shader when draw
//
