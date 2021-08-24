// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: p_enemy.h,v 1.1 1998/05/03 22:29:32 killough Exp $
//
//  BOOM, a modified and improved DOOM engine
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Enemy thinking, AI.
//      Action Pointer Functions
//      that are associated with states/frames.
//
//-----------------------------------------------------------------------------

#ifndef __P_ENEMY__
#define __P_ENEMY__

#include "p_mobj.h"

void P_NoiseAlert (mobj_t *target, mobj_t *emmiter);
void P_SpawnBrainTargets(void); // killough 3/26/98: spawn icon landings

// [XA] New mbf21 codepointers

void A_SpawnObject(mobj_t*);
void A_MonsterProjectile(mobj_t*);
void A_MonsterBulletAttack(mobj_t*);
void A_MonsterMeleeAttack(mobj_t*);
void A_RadiusDamage(mobj_t*);
void A_NoiseAlert(mobj_t*);
void A_HealChase(mobj_t*);
void A_SeekTracer(mobj_t*);
void A_FindTracer(mobj_t*);
void A_ClearTracer(mobj_t*);
void A_JumpIfHealthBelow(mobj_t*);
void A_JumpIfTargetInSight(mobj_t*);
void A_JumpIfTargetCloser(mobj_t*);
void A_JumpIfTracerInSight(mobj_t*);
void A_JumpIfTracerCloser(mobj_t*);
void A_JumpIfFlagsSet(mobj_t*);
void A_AddFlags(mobj_t*);
void A_RemoveFlags(mobj_t*);

extern struct brain_s {         // killough 3/26/98: global state of boss brain
  int easy, targeton;
} brain;

#endif // __P_ENEMY__
