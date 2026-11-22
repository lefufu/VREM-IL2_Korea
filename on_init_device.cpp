///////////////////////////////////////////////////////////////////////
//
// Reshade DCS VREM2 addon. VR Enhancer Mod for IDCS using reshade
// "hot" reload of mod possible using a Reshade addon as launcher (loaded with the game)
// and a dll containing the mod logic itselve. Mod settings are in uniforms of a technique
// 
// ----------------------------------------------------------------------------------------
//  on_init_device : not used finally, initially to create the constant buffer to be injected in modified shaders
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
#include "addon_injection.h"

#include "to_string.hpp"

using namespace reshade::api;


//*******************************************************************************
// update buffer with the current settings

/*
void write_settings(const ShaderInjectData& src)
{
	if (!g_ptr) return;
	for (int eye = 0; eye < 2; ++eye) {
		ShaderInjectData d = src;          // copie locale, on ne modifie pas la source
		d.eye = float(eye);
		memcpy(g_ptr + uint64_t(eye) * SLOT_SIZE, &d, sizeof(d));
	}
}
*/

/*void flush_settings()
{
	if (g_dirty.exchange(false)) write_settings();
}
*/

//*******************************************************************************
/*
void vrem_create_cb_buffer(device* dev)
{
	if (g_ptr) return;

	resource_desc desc(BUFFER_SIZE, memory_heap::cpu_to_gpu, resource_usage::constant_buffer);
	if (!dev->create_resource(desc, nullptr, resource_usage::cpu_access, &g_buf)) {
		reshade::log::message(reshade::log::level::info, "addon - create_cb_buffer : create_resource ECHEC; ");
		return;
	}

	void* p = nullptr;
	if (!dev->map_buffer_region(g_buf, 0, BUFFER_SIZE, map_access::write_only, &p) || !p) {
		reshade::log::message(reshade::log::level::info, "addon - create_cb_buffer : map_buffer_region ECHEC; ");
		dev->destroy_resource(g_buf);
		g_buf = {};
		return;
	}
	g_ptr = static_cast<uint8_t*>(p);

	g_range[0] = { g_buf, 0,         SLOT_SIZE };
	g_range[1] = { g_buf, SLOT_SIZE, SLOT_SIZE };

	write_settings(a_shared.cb_inject_values);

	std::stringstream s;
	s << "addon - create_cb_buffer : OK g_buf=0x" << std::hex << g_buf.handle
		<< " g_ptr=" << (void*)g_ptr << "; ";
	reshade::log::message(reshade::log::level::info, s.str().c_str());
}
*/
#ifdef _DEBUG
extern "C" {
#endif
	//*******************************************************************************
	VREM_EXPORT void vrem_on_init_device(device* dev) {

#if _DEBUG_LOGS
		log_init_device();
#endif
		// vrem_create_cb_buffer(dev);

		vrem_ensure_cb_buffer(dev);

		
	}
#ifdef _DEBUG
}
#endif

#ifdef _DEBUG
extern "C" {
#endif

	//*******************************************************************************
	VREM_EXPORT void vrem_on_destroy_device(device* dev) {

#if _DEBUG_LOGS
		log_destroy_device();
#endif
		
		vrem_destroy_cb_buffer(dev);
		/*
		
		if (g_buf.handle == 0) return;
		dev->unmap_buffer_region(g_buf);
		dev->destroy_resource(g_buf);
		g_buf = {};
		g_ptr = nullptr;
		g_range[0] = {};
		g_range[1] = {};
		*/

	}
#ifdef _DEBUG
}
#endif

