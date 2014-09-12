////////////////////////////////////////////////////////////////////////////
//	Module 		: derived_client_classes.h
//	Created 	: 16.08.2014
//  Modified 	: 22.08.2014
//	Author		: Alexander Petrov
//	Description : XRay derived client classes script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "base_client_classes.h"
#include "derived_client_classes.h"
#include "HUDManager.h"
#include "exported_classes_def.h"
#include "script_game_object.h"
#include "ui/UIDialogWnd.h"
#include "ui/UIInventoryWnd.h"
#include "../lua_tools.h"

/* ���������� � ����� �������� ������� � �������:
     * �������� �������� �������������� �� ����������� ���, ��� ��� �������� � ������ ������������ (*.ltx), � �� ��� ��� ��� ������� � ���������� ������
	 * ������ �������� �������������� �������� ����� �������� ��� game_object, �.� ��� ������������� ��������� ����. 
	    ��� ��������� ��������� ������ ����� ���������������� � �������� � �������� ����� ������ �� ������� ��������� ������ 1.0006.
   Alexander Petrov
*/


using namespace luabind;
#pragma optimize("s", on)

extern LPCSTR get_lua_class_name(luabind::object O);
extern CGameObject *lua_togameobject(lua_State *L, int index);

u32 get_zone_state(CCustomZone *obj)  { return (u32)obj->ZoneState(); }
void CAnomalyZoneScript::set_zone_state(CCustomZone *obj, u32 new_state)
{ 
	obj->SwitchZoneState ( (CCustomZone::EZoneState) new_state); 
}

void CAnomalyZoneScript::script_register(lua_State *L)
{
	module(L)
		[			
			class_<CCustomZone, CGameObject>("CCustomZone")
			.def  ("get_state_time"						,				&CCustomZone::GetStateTime) 
			.def  ("power"								,				&CCustomZone::Power)
			.def  ("relative_power"						,				&CCustomZone::RelativePower)


			.def_readwrite("attenuation"				,				&CCustomZone::m_fAttenuation) 
			.def_readwrite("effective_radius"			,				&CCustomZone::m_fEffectiveRadius) 
			.def_readwrite("hit_impulse_scale"			,				&CCustomZone::m_fHitImpulseScale) 
			.def_readwrite("max_power"					,				&CCustomZone::m_fMaxPower) 
			.def_readwrite("state_time"					,				&CCustomZone::m_iStateTime) 
			.def_readwrite("start_time"					,				&CCustomZone::m_StartTime) 
			.def_readwrite("time_to_live"				,				&CCustomZone::m_ttl) 
			.def_readwrite("zone_active"				,				&CCustomZone::m_bZoneActive) 
			 

			.property("radius"							,				&CCustomZone::Radius)			
			.property("zone_state"						,				&get_zone_state, &CAnomalyZoneScript::set_zone_state)

		];
}

IC void alive_entity_set_radiation(CEntityAlive *E, float value)
{
	E->SetfRadiation(value);
}

void CEntityScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CEntity, CGameObject>("CEntity")
			,
			class_<CEntityAlive, CEntity>("CEntityAlive")
			.property("radiation"						,			&CEntityAlive::g_Radiation, &alive_entity_set_radiation) // ���� � %
			.property("condition"						,           &CEntityAlive::conditions)
		];
}

void CEatableItemScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CEatableItem, CInventoryItem>("CEatableItem")
			.def_readwrite("eat_health"					,			&CEatableItem::m_fHealthInfluence)
			.def_readwrite("eat_power"					,			&CEatableItem::m_fPowerInfluence)
			.def_readwrite("eat_satiety"				,			&CEatableItem::m_fSatietyInfluence)
			.def_readwrite("eat_radiation"				,			&CEatableItem::m_fRadiationInfluence)
			.def_readwrite("eat_max_power"				,			&CEatableItem::m_fMaxPowerUpInfluence)
			.def_readwrite("wounds_heal_perc"			,			&CEatableItem::m_fWoundsHealPerc)
			.def_readwrite("eat_portions_num"			,			&CEatableItem::m_iPortionsNum)
			.def_readwrite("eat_max_power"				,			&CEatableItem::m_iStartPortionsNum)			
			,
			class_<CEatableItemObject, bases<CEatableItem, CGameObject>>("CEatableItemObject")
		];
}

void set_io_money(CInventoryOwner *IO, u32 money) { IO->set_money(money, true); }

CScriptGameObject  *item_lua_object(PIItem itm)
{	
	if (itm)
	{
		CGameObject *obj = smart_cast<CGameObject *>(itm);
		if (obj) return obj->lua_game_object();
	}
	return NULL;
}


CScriptGameObject  *inventory_active_item(CInventory *I)	{ return item_lua_object (I->ActiveItem()); }
CScriptGameObject  *inventory_selected_item(CInventory *I)
{
	CUIDialogWnd *IR = HUD().GetUI()->MainInputReceiver();
	if (!IR) return NULL;
	CUIInventoryWnd *wnd = smart_cast<CUIInventoryWnd *>(IR);
	if (!wnd) return NULL;
	if (wnd->GetInventory() != I) return NULL;		
	return item_lua_object( wnd->CurrentIItem() );	
}

CScriptGameObject  *get_inventory_target(CInventory *I)		{ return item_lua_object(I->m_pTarget); }

LPCSTR get_item_name				(CInventoryItem *I) { return I->Name(); }
LPCSTR get_item_name_short			(CInventoryItem *I) { return I->NameShort(); }


void item_to_belt(CInventory *I, lua_State *L)
{   // 1st param: CInventory*, 2nd param: item?
	CGameObject *obj = lua_togameobject(L, 2);
	if (NULL == obj) return;
	CInventoryItem *itm = smart_cast<CInventoryItem *>(obj);
	if (I && itm)
		I->Belt (itm);
}

void item_to_slot(CInventory *I, lua_State *L)
{   // 1st param: CInventory*, 2nd param: item?
	CGameObject *obj = lua_togameobject(L, 2);
	if (NULL == obj) return;
	CInventoryItem *itm = smart_cast<CInventoryItem *>(obj);
	if (I && itm)
		I->Slot(itm, !!lua_toboolean(L, 3));
}

void item_to_ruck(CInventory *I, lua_State *L)
{   // 1st param: CInventory*, 2nd param: item?
	CGameObject *obj = lua_togameobject(L, 2);
	if (NULL == obj) return;
	CInventoryItem *itm = smart_cast<CInventoryItem *>(obj);
	if (I && itm)
		I->Ruck(itm);
}

#ifdef INV_NEW_SLOTS_SYSTEM
void get_slots(luabind::object O)
{
	lua_State *L = O.lua_state();
	CInventoryItem *itm = luabind::object_cast<CInventoryItem*> (O);
	lua_createtable (L, 0, 0);
	int tidx = lua_gettop(L);
	if (itm)
	{
		for (u32 i = 0; i < itm->GetSlotsCount(); i++)
		{
			lua_pushinteger(L, i + 1); // key
			lua_pushinteger(L, itm->GetSlots()[i]);
			lua_settable(L, tidx);
		}
	}

}

void fake_set_slots(CInventoryItem *I, luabind::object T) { } // �������������� ����� �����, ���� GetSlots �� ����� ���������� ���������
#endif

void CInventoryScript::script_register(lua_State *L)
{
	module(L)
		[

			class_<CInventoryItem>("CInventoryItem")
			.def_readwrite("cost"						,			&CInventoryItem::m_cost)			
			.def_readonly("item_place"					,			&CInventoryItem::m_eItemPlace)
			.def_readwrite("item_condition"				,			&CInventoryItem::m_fCondition)
			.def_readwrite("inv_weight"					,			&CInventoryItem::m_weight)
			.property("class_name"						,			&get_lua_class_name)
			.property("item_name"						,			&get_item_name)
			.property("item_name_short"					,			&get_item_name_short)
			.property("slot"							,			&CInventoryItem::GetSlot, &CInventoryItem::SetSlot)
#ifdef INV_NEW_SLOTS_SYSTEM
			.property("slots"							,			&get_slots,    &fake_set_slots, raw(_2))	
#endif
			,
			class_<CInventoryItemObject, bases<CInventoryItem, CGameObject>>("CInventoryItemObject"),

			class_ <CInventory>("CInventory")
			.def_readwrite("max_belt"					,			&CInventory::m_iMaxBelt)
			.def_readwrite("max_weight"					,			&CInventory::m_fMaxWeight)
			.def_readwrite("take_dist"					,			&CInventory::m_fTakeDist)
			.def_readonly ("total_weight"				,			&CInventory::m_fTotalWeight)
			.property	  ("active_item"				,			&inventory_active_item)
			.property	  ("selected_item"				,			&inventory_selected_item)
			.property	  ("target"						,			&get_inventory_target)
			.property	  ("class_name"					,			&get_lua_class_name)
			.def		  ("to_belt"					,			&item_to_slot,   raw(_2))
			.def		  ("to_slot"					,			&item_to_slot,   raw(_2))
			.def		  ("to_ruck"					,			&item_to_ruck,   raw(_2))
			,
			class_<CInventoryOwner>("CInventoryOwner")
			.def_readonly ("inventory"					,			&CInventoryOwner::m_inventory)
			.def_readonly ("talking"					,			&CInventoryOwner::m_bTalking)
			.def_readwrite("allow_talk"					,			&CInventoryOwner::m_bAllowTalk)
			.def_readwrite("allow_trade"				,			&CInventoryOwner::m_bAllowTrade)
			.def_readwrite("raw_money"					,			&CInventoryOwner::m_money)
			.property	  ("money"						,			&CInventoryOwner::get_money,				&set_io_money)			
			.property	  ("class_name"					,			&get_lua_class_name)			
			
		];
}

void CMonsterScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CBaseMonster, bases<CInventoryOwner, CEntityAlive>>("CBaseMonster")
			.def_readwrite("agressive"					,			&CBaseMonster::m_bAggressive)
			.def_readwrite("angry"						,			&CBaseMonster::m_bAngry)
			.def_readwrite("damaged"					,			&CBaseMonster::m_bDamaged)
			.def_readwrite("grownlig"					,			&CBaseMonster::m_bGrowling)						
			.def_readwrite("run_turn_left"				,			&CBaseMonster::m_bRunTurnLeft)
			.def_readwrite("run_turn_right"				,			&CBaseMonster::m_bRunTurnRight)
			.def_readwrite("sleep"						,			&CBaseMonster::m_bSleep)
			.def_readwrite("state_invisible"			,			&CBaseMonster::state_invisible)
		];
}


int curr_fire_mode(CWeaponMagazined *wpn) { return wpn->GetCurrentFireMode(); }



void COutfitScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CCustomOutfit, CInventoryItemObject>("CCustomOutfit")
			.def_readwrite("additional_inventory_weight"		,		&CCustomOutfit::m_additional_weight)
			.def_readwrite("additional_inventory_weight2"		,		&CCustomOutfit::m_additional_weight2)
			.def_readwrite("power_loss"							,		&CCustomOutfit::m_fPowerLoss)
			// alpet: ���������� � ��������� ��������� ��� ��������� ����.
			.property("burn_protection"					,			&get_protection<u8[ALife::eHitTypeBurn	  + 1]>			, 	&set_protection<u8[ALife::eHitTypeBurn	  + 1]>)			
			.property("strike_protection"				,			&get_protection<u8[ALife::eHitTypeStrike  + 1]>			,	&set_protection<u8[ALife::eHitTypeStrike  + 1]>)
			.property("shock_protection"				,			&get_protection<u8[ALife::eHitTypeShock   + 1]>			,	&set_protection<u8[ALife::eHitTypeShock   + 1]>)
			.property("wound_protection"				,			&get_protection<u8[ALife::eHitTypeWound   + 1]>			,	&set_protection<u8[ALife::eHitTypeWound   + 1]>)
			.property("radiation_protection"			,			&get_protection<u8[ALife::eHitTypeRadiation+ 1]>		,	&set_protection<u8[ALife::eHitTypeRadiation + 1]>)
			.property("telepatic_protection"			,			&get_protection<u8[ALife::eHitTypeTelepatic+ 1]>		,	&set_protection<u8[ALife::eHitTypeTelepatic + 1]>)
			.property("chemical_burn_protection"		,			&get_protection<u8[ALife::eHitTypeChemicalBurn + 1]>	,	&set_protection<u8[ALife::eHitTypeChemicalBurn + 1]>)
			.property("explosion_protection"			,			&get_protection<u8[ALife::eHitTypeExplosion + 1]>		,	&set_protection<u8[ALife::eHitTypeExplosion + 1]>)
			.property("fire_wound_protection"			,			&get_protection<u8[ALife::eHitTypeFireWound + 1]>		,	&set_protection<u8[ALife::eHitTypeFireWound + 1]>)
			.property("wound_2_protection"				,			&get_protection<u8[ALife::eHitTypeWound_2 + 1]>			,	&set_protection<u8[ALife::eHitTypeWound_2   + 1]>)			
			.property("physic_strike_protection"		,			&get_protection<u8[ALife::eHitTypePhysicStrike + 1]>	,	&set_protection<u8[ALife::eHitTypePhysicStrike + 1]>)			
		];

}

void CWeaponScript::script_register(lua_State *L)
{
	module(L)
		[
			class_<CWeapon,	CInventoryItemObject>		("CWeapon")
			// �� ����������������� ������ CHudItemObject:
			.property("state", &CHudItemObject::GetState)
			.property("next_state", &CHudItemObject::GetNextState)
			// ============================================================================= //
			.def_readwrite("ammo_mag_size"				,			&CWeapon::iMagazineSize)
			.def_readwrite("scope_dynamic_zoom"			,			&CWeapon::m_bScopeDynamicZoom)
			.def_readwrite("zoom_enabled"				,			&CWeapon::m_bZoomEnabled)
			.def_readwrite("zoom_factor"				,			&CWeapon::m_fZoomFactor)
			.def_readwrite("zoom_rotate_time"			,			&CWeapon::m_fZoomRotateTime)
			.def_readwrite("iron_sight_zoom_factor"		,			&CWeapon::m_fIronSightZoomFactor)
			.def_readwrite("scrope_zoom_factor"			,			&CWeapon::m_fScopeZoomFactor)
			// ���������� ��� ���������� ��������� ������� �� ��������:
			
			.def_readwrite("grenade_launcher_x"			,			&CWeapon::m_iGrenadeLauncherX)
			.def_readwrite("grenade_launcher_y"			,			&CWeapon::m_iGrenadeLauncherY)
			.def_readwrite("scope_x"					,			&CWeapon::m_iScopeX)
			.def_readwrite("scope_y"					,			&CWeapon::m_iScopeY)
			.def_readwrite("silencer_x"					,			&CWeapon::m_iSilencerX)
			.def_readwrite("silencer_y"					,			&CWeapon::m_iSilencerY)

			.def_readonly("misfire"						,			&CWeapon::bMisfire)
			.def_readonly("zoom_mode"					,			&CWeapon::m_bZoomMode)
			

			.property("ammo_elapsed"					,			&CWeapon::GetAmmoElapsed, &CWeapon::SetAmmoElapsed)
			.def("get_ammo_current"						,			&CWeapon::GetAmmoCurrent)
			//.def("load_config"						,			&CWeapon::Load)
			.def("start_fire"							,			&CWeapon::FireStart)
			.def("stop_fire"							,			&CWeapon::FireEnd)
			.def("start_fire2"							,			&CWeapon::Fire2Start)			// ����� ����� - ������ �������? )
			.def("stop_fire2"							,			&CWeapon::Fire2End)
			.def("stop_shoothing"						,			&CWeapon::StopShooting)

			,
			class_<CWeaponMagazined, CWeapon>			("CWeaponMagazined")
			.def_readonly("shot_num"					,			&CWeaponMagazined::m_iShotNum)
			.def_readwrite("queue_size"					,			&CWeaponMagazined::m_iQueueSize)
			.def_readwrite("shoot_effector_start"		,			&CWeaponMagazined::m_iShootEffectorStart)
			.def_readwrite("cur_fire_mode"				,			&CWeaponMagazined::m_iCurFireMode)			
			.property	  ("fire_mode"					,			&curr_fire_mode)
			,
			class_<CWeaponMagazinedWGrenade,			CWeaponMagazined>("CWeaponMagazinedWGrenade")
			.def_readwrite("gren_mag_size"				,			&CWeaponMagazinedWGrenade::iMagazineSize2)			
			,
			class_<CMissile, CInventoryItemObject>		("CMissile")
			.def_readwrite("destroy_time"				,			&CMissile::m_dwDestroyTime)
			.def_readwrite("destroy_time_max"			,			&CMissile::m_dwDestroyTimeMax)			
			
		];
}