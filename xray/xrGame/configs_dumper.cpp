#include "stdafx.h"
#include "configs_dumper.h"
#include "configs_common.h"
#include "../xrCore/ppmd_compressor.h"
#include "../xrCore/xr_ini.h"

#include "GameObject.h"
#include "level.h"
#include "actor_mp_client.h"
#include "inventory.h"
#include "weapon.h"
#include "game_cl_mp.h"

namespace mp_anticheat
{

configs_dumper::configs_dumper()
{
	m_state								= ds_not_active;
	m_buffer_for_compress				= NULL;
	m_buffer_for_compress_size			= 0;
	m_buffer_for_compress_capacity		= 0;

	m_make_start_event					= NULL;
	m_make_done_event					= NULL;
}

configs_dumper::~configs_dumper()
{
	if (m_make_start_event)
	{
		SetEvent(m_make_start_event);
		WaitForSingleObject(m_make_done_event, INFINITE);	//thread stoped
		CloseHandle(m_make_done_event);
		CloseHandle(m_make_start_event);
	}
	xr_free(m_buffer_for_compress);
}

void configs_dumper::shedule_Update(u32 dt)
{
	DWORD thread_result = WaitForSingleObject(m_make_done_event, 0);
	R_ASSERT((thread_result != WAIT_ABANDONED) && (thread_result != WAIT_FAILED));
	R_ASSERT(m_state == ds_active);
	if (thread_result == WAIT_OBJECT_0)
	{
		m_complete_cb	(m_buffer_for_compress, m_buffer_for_compress_size, m_dump_result.size());
		m_state			= ds_not_active;
		Engine.Sheduler.Unregister(this);
	}
}

typedef	buffer_vector<IAnticheatDumpable const *>	active_objects_t;
static active_objects_t::size_type get_active_objects(active_objects_t & dest, u32 const dest_capacity)
{
	return 0;
}

static active_objects_t::size_type const max_active_objects = 16;

void configs_dumper::write_configs()
{
	long i							= 0;
	m_dump_result.clear				();
	m_ltx_configs.start_dump		();
	if (m_yield_cb)
	{
		while (m_ltx_configs.dump_one(m_dump_result))
		{
			m_yield_cb(i);
			++i;
		}
	} else
	{
		while (m_ltx_configs.dump_one(m_dump_result)) {};
	}
	CInifile			active_params_dumper(NULL, FALSE, FALSE, FALSE);
	active_objects_t	active_objects(
		_alloca(sizeof(active_objects_t::value_type) * max_active_objects),
		max_active_objects);
	active_objects_t::size_type	aobjs_count	= get_active_objects(active_objects, max_active_objects);
	string16 tmp_strbuff;
	for (active_objects_t::size_type i = 0; i < aobjs_count; ++i)
	{
		sprintf_s				(tmp_strbuff, "%d", i + 1);
		m_active_params.dump	(active_objects[i], tmp_strbuff, active_params_dumper);
	}
	active_params_dumper.save_as	(m_dump_result);
}

char const * cd_info_secion			= "config_dump_info";
char const * cd_player_name_key		= "player_name";
char const * cd_player_digest_key	= "player_digest";
char const * cd_digital_sign_key	= "digital_sign";
char const * cd_creation_date		= "creation_date";


void configs_dumper::dump_config(complete_callback_t complete_cb)
{
}

void configs_dumper::compress_configs	()
{
	realloc_compress_buffer(m_dump_result.size());
	ppmd_yield_callback_t ts_cb;
	if (m_yield_cb)
	{
		ts_cb.bind(this, &configs_dumper::switch_thread);
	}
	m_buffer_for_compress_size = ppmd_compress_mt(
		m_buffer_for_compress,
		m_buffer_for_compress_capacity,
		m_dump_result.pointer(),
		m_dump_result.size(),
		ts_cb
	);
}

void configs_dumper::dumper_thread(void* my_ptr)
{
	configs_dumper* this_ptr	= static_cast<configs_dumper*>(my_ptr);
	DWORD wait_result = WaitForSingleObject(this_ptr->m_make_start_event, INFINITE);
	while ((wait_result != WAIT_ABANDONED) || (wait_result != WAIT_FAILED))
	{
		break;				// error
	}
	SetEvent(this_ptr->m_make_done_event);
}

void __stdcall	configs_dumper::yield_cb(long progress)
{
	if (progress % 5 == 0)
	{
		switch_thread();
	}
}

void __stdcall configs_dumper::switch_thread()
{
	if (!SwitchToThread())
			Sleep(10);
}

void configs_dumper::realloc_compress_buffer(u32 need_size)
{
}

}//namespace mp_anticheat