/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/Inventory.h 1     12/31/06 11:28a Lee $
//
//	File: Inventory.h
//
/////////////////////////////////////////////////////////////////////////////


#pragma once


enum InvType_t
{
	InvType_Weapon,
	InvType_
};


enum WeaponAnim_t
{
	WeaponAnim_None,
	WeaponAnim_Axe,
	WeaponAnim_Hammer,
	WeaponAnim_Mace,
	WeaponAnim_Staff,
	WeaponAnim_Star,
	WeaponAnim_Sword
};


class CwaInvObject
{
public:
	CwaInvObject(void);
	~CwaInvObject(void);

	void GetName(char name[]);
};


class CwaInvList
{
public:
};



