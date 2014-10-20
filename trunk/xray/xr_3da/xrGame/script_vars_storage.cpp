////////////////////////////////////////////////////////////////////////////
//	Module 		: script_vars_storage.cpp
//	Created 	: 19.10.2014
//  Modified 	: 19.10.2014
//	Author		: Alexander Petrov
//	Description : global script vars class, with saving content to savegame
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "pch_script.h"
#include "alife_space.h"
#include "script_vars_storage.h"
#include "script_net_packet.h"
#include "script_engine.h"
#include "../lua_tools.h"
#include "../../xrNetServer/NET_utils.h"

#pragma todo("alpet: ����� ������� �������� �����������")
#pragma optimize("gyts", off)  
 
CScriptVarsStorage g_ScriptVars;

#define  SMALL_VAR_SIZE	  sizeof(SCRIPT_VAR::s_value)

void SCRIPT_VAR::release()
{
	SCRIPT_VAR sv = *this;
	ZeroMemory(this, sizeof(SCRIPT_VAR));

	if (LUA_TTABLE == sv.eff_type())
	{		
		xr_delete(sv.T);
		return;
	}

	if (LUA_TNETPACKET == sv.eff_type())
	{		
		xr_delete(sv.P);
		return;
	}
		
	if (sv.type & SVT_ALLOCATED)
		xr_free(sv.data);	
}

void* SCRIPT_VAR::smart_alloc(int new_type, u32 cb) // ����� ��������������� ��������� ������
{
	// autodetect embedded var-size
	switch (type & 0xffff)
	{
	case LUA_TBOOLEAN: 
		cb = sizeof(b_value); break;
	case LUA_TNUMBER:
		cb = sizeof(n_value); break;	
	}

	size = cb;

	if (type & SVT_ALLOCATED)
	{
		if (size > SMALL_VAR_SIZE)
			data = xr_realloc(data, size);
		else
			xr_free(data);
	}
	else
	{
		if (size > SMALL_VAR_SIZE)
			data = xr_malloc(size);	
	}

	if (size > SMALL_VAR_SIZE)
	{
		type = new_type | SVT_ALLOCATED;
		return data;
	}
	else
	{
		type = new_type;
		return &s_value;
	}
}

CScriptVarsTable::~CScriptVarsTable()
{
	clear();
}

void CScriptVarsTable::clear()
{
	auto it = m_map.begin();
	auto end = m_map.end();
	for (; it != end; it++)
		it->second.release();					
	m_map.clear();
}

void CScriptVarsTable::load(IReader  &memory_stream)
{
	clear();
	u32 var_count = memory_stream.r_u32();
	if (!var_count) return; // noting for load
	is_array = true;
	u32 loaded = 0;
	while (var_count > 0 && !memory_stream.eof())  
	{
		shared_str var_name;
		SCRIPT_VAR sv;
		memory_stream.r_stringZ(var_name);
		sv.type = (int)memory_stream.r_u32();
		if (LUA_TTABLE == sv.eff_type())
		{			
			sv.T = xr_new<CScriptVarsTable>();
			sv.T->is_array = (sv.type & SVT_ARRAY_TABLE) > 0; 
			sv.T->load(memory_stream);
			m_map[var_name] = sv;
			continue;
		}

		sv.size = memory_stream.r_u32();

		if (LUA_TNETPACKET == sv.eff_type()	)
		{
			sv.P = xr_new<NET_Packet>();
			sv.P->B.count = sv.size;
			sv.P->r_seek(memory_stream.r_u32());			
			memory_stream.r (sv.P->B.data, sv.size);		
			m_map[var_name] = sv;
			continue;
		}
		
		{  // default var type
			if (sv.size > sizeof(sv.s_value))
			{
				sv.data = xr_malloc(sv.size);
				memory_stream.r(sv.data, sv.size);
			}
			else
				memory_stream.r(&sv.s_value, sv.size);

			m_map[var_name] = sv;
		}
		var_count--;
		loaded++;
		shared_str test;
		test.sprintf("%d", atoi(*var_name));
		if (test != var_name)
			is_array = false;
	}

}

void CScriptVarsTable::save(IWriter  &memory_stream)
{
	memory_stream.w_u32(m_map.size());
	auto it = m_map.begin();
	auto end = m_map.end();
	for (; it != end; it++)
	{
		SCRIPT_VAR &sv = it->second;
		memory_stream.w_stringZ(it->first);
		
		if (LUA_TTABLE == sv.eff_type())
		{
			if (sv.T->is_array)
				sv.type |= SVT_ARRAY_TABLE;
			else
				sv.type = LUA_TTABLE;

			memory_stream.w_u32((u32) sv.type);
			sv.T->save(memory_stream);
			continue;
		}

		memory_stream.w_u32((u32) sv.type);

		if (LUA_TNETPACKET == sv.eff_type())
		{			
			memory_stream.w_u32(sv.P->B.count); 
			memory_stream.w_u32(sv.P->r_tell());			
			memory_stream.w (&sv.P->B.data, sv.P->B.count);
			continue;
		}
		memory_stream.w_u32(sv.size); // sizeof *data
		if (sv.size > sizeof(sv.s_value))
			memory_stream.w (sv.data, sv.size);
		else
			memory_stream.w (&sv.s_value, sv.size);
	}

}

void CScriptVarsStorage::load(IReader  &memory_stream)
{
	if (!memory_stream.find_chunk(SCRIPT_VARS_CHUNK_DATA))
		 return;
	inherited::load(memory_stream);
	Msg	("* %d script vars are successfully loaded", size());
}

void CScriptVarsStorage::save(IWriter  &memory_stream)
{
	if (0 == map().size()) return;

	memory_stream.open_chunk	(SCRIPT_VARS_CHUNK_DATA);
	inherited::save(memory_stream);
	memory_stream.close_chunk();
	
	Msg	("* %d script vars are successfully saved", size());
}



using namespace luabind;
using namespace luabind::detail;



void register_method(lua_State *L, char *key, int mt_index, lua_CFunction f)
{	
	lua_pushcfunction(L, f);	
	lua_setfield(L, mt_index, key);
}

CScriptVarsTable *lua_tosvt(lua_State *L, int index)
{
	if (!lua_isuserdata(L, index))
		return NULL;

	SCRIPT_VAR *sv = (SCRIPT_VAR*)lua_touserdata(L, index);
	return sv->T;
}

int script_vars_dump (lua_State *L, CScriptVarsTable *svt, bool unpack) // ���� ���� � �������� ���������� � �������
{	
	if (svt)
	{
		lua_createtable(L, 0, svt->size());
		int tidx = lua_gettop(L);
		if (tidx > 100)
		{
			log_script_error("script_vars_dump:  to many nested tables in dump");
			return 1;
		}

		auto it = svt->map().begin();
		int i = 1;
		for (; it != svt->map().end(); it++)
		{			
			LPCSTR key = *it->first;
			if (svt->is_array)
				lua_pushinteger(L, atoi(key));
			else
				lua_pushstring(L, key);
			svt->get(L, key, unpack);
			lua_settable(L, tidx);
		}
	}
	else
		lua_pushnil(L);	

	return 1;
}
int script_vars_dump(lua_State *L) // ���� ���� � �������� ���������� � �������
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);	
	return script_vars_dump (L, svt, !!lua_toboolean(L, 2));
}


int script_vars_size(lua_State *L)
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);	
	lua_pushinteger(L, svt ? svt->size() : 0);
	return 1;
}

void CScriptVarsTable::get(lua_State *L, LPCSTR k, bool unpack)
{
	shared_str key (k);

	auto it = map().find(key);
	if (it != map().end())
	{
		SCRIPT_VAR &sv = it->second;
		void *p;

		switch (sv.eff_type())
		{
		case LUA_TBOOLEAN:
			lua_pushboolean(L, sv.b_value);
			break;
		case LUA_TNUMBER:
			lua_pushnumber(L, sv.n_value);
			break;	
		case LUA_TSTRING:
			if (sv.size <= sizeof(sv.s_value))
				lua_pushstring(L, sv.s_value);
			else
				lua_pushstring(L, (char*)sv.data);
			break;
		case LUA_TUSERDATA:
			p = lua_newuserdata(L, sv.size);
			memcpy(p, sv.data, sv.size);
			break;			
		case LUA_TTABLE:
			if (unpack)
				script_vars_dump (L, sv.T, unpack);
			else			
				lua_pushsvt(L, sv.T);
			break;
		case LUA_TNETPACKET:
			convert_to_lua<NET_Packet*>(L, sv.P); 
			break;
		};
	}
	else
		lua_pushnil(L);
}


void CScriptVarsTable::set(lua_State *L, LPCSTR k, int index)
{
	shared_str key(k);	
	SCRIPT_VAR sv;
	
	bool exists = false;
	auto it = map().find(key);
	if (it != map().end())
	{
		sv = it->second;
		exists = true;
	}
	else
		ZeroMemory(&sv, sizeof(sv));
	
	int new_type = lua_type(L, index);
	
	switch (new_type)
	{
	case LUA_TNIL:
		if (exists)
			map().erase(it);
		sv.release();
		return;		
	case LUA_TBOOLEAN:
		sv.smart_alloc(new_type);
		sv.b_value = !!lua_toboolean(L, index);		
		break;
	case LUA_TNUMBER:
		sv.smart_alloc(new_type);
		sv.n_value = lua_tonumber(L, index);		
		break;
	case LUA_TSTRING:
	{
		const char *s = lua_tostring(L, index);
		char *dst = (char*)sv.smart_alloc(new_type, xr_strlen(s) + 1);
		strcpy_s(dst, sv.size, s); // ���������� �������� ����� �� 7 ��������		
		break;
	}
	case LUA_TTABLE:
	{		
		// ���������� ������� � �������� ���������� (�������� �������������)
		if (new_type != sv.type)
		{
			sv.release();
			sv.type = new_type;
			sv.size = 4;
			sv.T = xr_new<CScriptVarsTable>();
		}
		sv.T->clear();
		int save_top = lua_gettop(L);
		if (size_t count = lua_objlen(L, index))  // ��������, ��� ��� ������
		{
			string16  tmp;
			for (size_t i = 1; i <= count; i++)
			{
				lua_pushinteger (L, i);
				lua_gettable(L, index);
				sv.T->set(L, itoa(i, tmp, 10), lua_gettop(L));
				lua_pop(L, 1);
			}
			sv.T->is_array = true;
		}
		else
		{
			lua_pushnil(L);  // ������ �������� �������		
			while (lua_next(L, index) != 0)
			{
				const char* tk = lua_tostring(L, -2);
				sv.T->set(L, tk, lua_gettop(L));
				lua_pop(L, 1); // ������� �������� �� �����
			}
		}
		lua_settop(L, save_top);

		break;
	}
	case LUA_TUSERDATA:
	{
		object_rep* rep = is_class_object(L, index);
		if (rep && strstr(rep->crep()->name(), "net_packet"))
		{
			sv.smart_alloc(LUA_TNETPACKET); // ��������� ������ ������
			NET_Packet *src = (NET_Packet *)rep->ptr();
			sv.size = src->B.count;			
			sv.P = xr_new<NET_Packet>();
			sv.P->B.count = sv.size;
			sv.P->r_seek(src->r_tell());			
			memcpy_s(sv.P->B.data, sizeof(sv.P->B.data), src->B.data, sv.size);
			map()[key] = sv;
			return;
		}
		sv.size = 1;
		if (lua_gettop(L) > index)
			sv.size = lua_tointeger(L, index + 1);
		else
			sv.size = lua_objlen(L, index);
		void *dst = sv.smart_alloc(new_type, sv.size); // ��������� ������ ������		
		memcpy_s(dst, sv.size, lua_touserdata(L, index), sv.size);
		break;
	}
	default:
		log_script_error("script_vars not supported lua type %d for var %s", sv.type, *key);
		if (exists) 
			map().erase(it);
		sv.release();
		break;
	};
	
	map()[key] = sv;
}


int script_vars_index(lua_State *L) // ������ ���������� �� �������� �� ���������� �������
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);

	LPCSTR k = lua_tostring(L, 2);
	
	if (!k || !xr_strlen(k) || !svt)
	{
		lua_pushnil(L);
		return 1;
	}
	
	svt->get(L, k, false);
	return 1;
}


int script_vars_new_index(lua_State *L) // ������ ���������� �� �������� �� ���������� �������
{
	CScriptVarsTable *svt = lua_tosvt(L, 1);
	LPCSTR k = lua_tostring(L, 2);
	if (!k || !xr_strlen(k) || !svt)
		return 0;

	svt->set(L, k, 3);
	return 0;
}


void lua_pushsvt(lua_State *L, CScriptVarsTable *T)
{
 	SCRIPT_VAR *sv = (SCRIPT_VAR*) lua_newuserdata(L, sizeof(SCRIPT_VAR));	
	sv->type = LUA_TTABLE;
	sv->size = 4;
	sv->T = T;

	int value_index = lua_gettop(L);
	luaL_newmetatable(L, "GMT_SCRIPT_VARS");		
	int mt = lua_gettop(L);
	register_method(L, "__len",       mt, script_vars_size);	
	register_method(L, "__call",      mt, script_vars_dump);
	register_method(L, "__index",     mt, script_vars_index);	
    register_method(L, "__newindex",  mt, script_vars_new_index);
	lua_pushvalue(L, mt);
	lua_setfield (L, mt, "__metatable");
	// Msg(" stack pos 1 = %d ", lua_gettop(L));	
	lua_setmetatable(L, value_index);
	// Msg(" stack pos 2 = %d ", lua_gettop(L));

}


int get_stored_vars(lua_State *L)
{
	lua_pushsvt(L, &g_ScriptVars);
	return 1;
}

void CScriptVarsStorage::script_register(lua_State *L)
{
	lua_pushcfunction(L, &get_stored_vars);
	lua_setglobal(L, "get_stored_vars");
}