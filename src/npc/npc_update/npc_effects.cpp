/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2026 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cmath>

#include "sdl_proxy/sdl_stdinc.h"

#include "globals.h"
#include "npc.h"
#include "npc_traits.h"
#include "npc/npc_queues.h"
#include "config.h"
#include "collision.h"
#include "layers.h"
#include "editor.h"
#include "player.h"

#include "main/trees.h"

// The native emergence render clips sprites while they are still inside the
// block. Keep that phase centred on the block, then fan out as complete
// sprites. The final motion is an eased arc rather than a horizontal slide.
static inline int s_multiplayerEmergenceHeight(const NPC_t& n)
{
    return (n.Effect == NPCEFF_EMERGE_DOWN)
        ? 32 : ((n->HeightGFX > 0) ? n->HeightGFX : n->THeight);
}

static inline int s_multiplayerPopFrames()
{
    return 20;
}

static inline double s_multiplayerPopHeight(const NPC_t& n)
{
    // Leaves should feel light, mounts have weight, flowers settle quietly.
    if(n.Type == NPCID_LEAF_POWER)
        return 26.0;
    if(n.Type == NPCID_ITEM_POD || n.Type == NPCID_GRN_BOOT ||
       n.Type == NPCID_RED_BOOT || n.Type == NPCID_BLU_BOOT)
        return 18.0;
    if(n.Type == NPCID_FIRE_POWER_S1 || n.Type == NPCID_FIRE_POWER_S3 ||
       n.Type == NPCID_FIRE_POWER_S4 || n.Type == NPCID_ICE_POWER_S3 ||
       n.Type == NPCID_ICE_POWER_S4)
        return 15.0;
    return 22.0; // mushrooms and other supported power-ups
}

static inline double s_multiplayerPopOffset(const NPC_t& n, double progress)
{
    const double t = std::max(0.0, std::min(1.0, progress));
    const double pi = 3.14159265358979323846;
    // Main leap occupies 72% of the animation. A much smaller second
    // hop makes the landing feel springy, with no final position jump.
    if(t < 0.72)
        return s_multiplayerPopHeight(n) * std::sin(pi * t / 0.72);

    double bounce = 4.5;
    if(n.Type == NPCID_LEAF_POWER)
        bounce = 6.0;
    else if(n.Type == NPCID_FIRE_POWER_S1 || n.Type == NPCID_FIRE_POWER_S3 ||
            n.Type == NPCID_FIRE_POWER_S4 || n.Type == NPCID_ICE_POWER_S3 ||
            n.Type == NPCID_ICE_POWER_S4)
        bounce = 2.0;

    return bounce * std::sin(pi * (t - 0.72) / 0.28);
}

static inline void s_updateMultiplayerEmergence(NPC_t& n)
{
    if(!n.multiplayerPowerupEmerging)
        return;

    const int emergeHeight = s_multiplayerEmergenceHeight(n);
    const double progress = double(n.Effect2 - emergeHeight) /
        s_multiplayerPopFrames();
    const double t = std::max(0.0, std::min(1.0, progress));

    // Every sprite fully clears the real block before travelling sideways.
    // Ease the horizontal movement so the item moves naturally along its hop.
    const double eased = t * t * (3.0 - 2.0 * t);
    n.Location.X = n.multiplayerPowerupOriginX +
        (n.multiplayerPowerupTargetX - n.multiplayerPowerupOriginX) * eased;

    // Add only the change in vertical offset per frame; this returns each
    // NPC precisely to its emergence height at the end of the bounce.
    const double previousProgress = double(n.Effect2 - 1 - emergeHeight) /
        s_multiplayerPopFrames();
    const double shift = s_multiplayerPopOffset(n, progress) -
        s_multiplayerPopOffset(n, previousProgress);
    n.Location.Y += (n.Effect == NPCEFF_EMERGE_DOWN ? 1.0 : -1.0) * shift;
}

static inline void s_finishMultiplayerEmergence(NPC_t& n)
{
    if(!n.multiplayerPowerupEmerging)
        return;
    n.multiplayerPowerupEmerging = false;
    if(n.multiplayerLeafPop)
    {
        // A lively upward pop that transitions into native leaf flutter AI.
        n.Location.SpeedY = -7.5;
        const double delta = n.multiplayerPowerupTargetX - n.multiplayerPowerupOriginX;
        n.Location.SpeedX = (delta < 0 ? -1.3 : 1.3);
        n.Direction = (delta < 0 ? -1 : 1);
        n.Special = 0;
        n.multiplayerLeafPop = false;
    }
}

static inline void NPCEffectLogic_EmergeUp(int A)
{
    if(NPC[A].multiplayerPowerupEmerging && NPC[A].Effect2 < 0)
    {
        ++NPC[A].Effect2;
        return;
    }
    // After the full sprite has cleared the block, hold its full height
    // and animate the fan-out without stretching or clipping it.
    if(NPC[A].multiplayerPowerupEmerging)
    {
        const int emergeHeight = s_multiplayerEmergenceHeight(NPC[A]);
        s_updateMultiplayerEmergence(NPC[A]);
        if(NPC[A].Effect2 >= emergeHeight)
        {
            if(NPC[A].Effect2 < emergeHeight + s_multiplayerPopFrames())
            {
                ++NPC[A].Effect2;
                return;
            }
            // The full-size sprite has finished its arc. Transition to
            // normal gameplay without adding an extra reveal step.
            NPC[A].Effect = NPCEFF_NORMAL;
            NPC[A].Effect2 = 0;
            s_finishMultiplayerEmergence(NPC[A]);
            NPC[A].Location.Y += NPC[A].Location.Height;
            NPC[A].Location.Height = NPC[A]->THeight;
            NPC[A].Location.Y -= NPC[A].Location.Height;
            return;
        }
    }
    if(NPC[A].Direction == 0.0f) // Move toward the closest player
    {
        double C = 0;
        for(int B = 1; B <= numPlayers; B++)
        {
            if(!Player[B].Dead && Player[B].Section == NPC[A].Section)
            {
                double dist = NPCPlayerTargetDist(NPC[A], Player[B]);
                if(C == 0.0 || dist < C)
                {
                    C = dist;
                    NPC[A].Direction = -Player[B].Direction;
                }
            }
        }
    }

    NPC[A].Frame = EditorNPCFrame(NPC[A].Type, NPC[A].Direction, A);
    const int emergeStep = NPC[A].multiplayerPowerupEmerging ? 2 : 1;
    NPC[A].Effect2 += emergeStep;
    NPC[A].Location.Y -= emergeStep;
    NPC[A].Location.Height += emergeStep;

    if(NPC[A]->HeightGFX > 0)
    {
        if(NPC[A].Effect2 >= NPC[A]->HeightGFX &&
           !NPC[A].multiplayerPowerupEmerging)
        {

            NPC[A].Effect = NPCEFF_NORMAL;
            NPC[A].Effect2 = 0;
            s_finishMultiplayerEmergence(NPC[A]);
            NPC[A].Location.Y += NPC[A].Location.Height;
            NPC[A].Location.Height = NPC[A]->THeight;
            NPC[A].Location.Y += -NPC[A].Location.Height;
        }
    }
    else
    {
        if(NPC[A].Effect2 >= NPC[A]->THeight &&
           !NPC[A].multiplayerPowerupEmerging)
        {

            NPC[A].Effect = NPCEFF_NORMAL;
            NPC[A].Effect2 = 0;
            s_finishMultiplayerEmergence(NPC[A]);
            NPC[A].Location.Height = NPC[A]->THeight;
        }
    }
}

static inline void NPCEffectLogic_EmergeDown(int A)
{
    if(NPC[A].multiplayerPowerupEmerging && NPC[A].Effect2 < 0)
    {
        ++NPC[A].Effect2;
        return;
    }
    if(NPC[A].multiplayerPowerupEmerging && NPC[A].Location.Height == 0)
        NPC[A].Location.Height = NPC[A]->THeight;
    s_updateMultiplayerEmergence(NPC[A]);
    if(NPC[A].multiplayerPowerupEmerging &&
       NPC[A].Effect2 >= 32)
    {
        if(NPC[A].Effect2 < 32 + s_multiplayerPopFrames())
        {
            ++NPC[A].Effect2;
            return;
        }
        // Finished the pop with the entire sprite visible.
        // Continue through the existing downward-emergence collision fixup.
    }
    if(NPC[A].Type == NPCID_LEAF_POWER)
        NPC[A].Direction = 1;
    else if(NPC[A].Direction == 0.f) // Move toward the closest player
    {
        double C = 0;
        for(int B = 1; B <= numPlayers; B++)
        {
            if(!Player[B].Dead && Player[B].Section == NPC[A].Section)
            {
                double dist = NPCPlayerTargetDist(NPC[A], Player[B]);
                if(C == 0.0 || dist < C)
                {
                    C = dist;
                    NPC[A].Direction = -Player[B].Direction;
                }
            }
        }
    }

    const int emergeStep = NPC[A].multiplayerPowerupEmerging ? 2 : 1;
    NPC[A].Effect2 += emergeStep;
    NPC[A].Location.Y += emergeStep;

    if(NPC[A].Effect2 >= 32)
    {
        NPC[A].Effect = NPCEFF_NORMAL;
        NPC[A].Effect2 = 0;
        s_finishMultiplayerEmergence(NPC[A]);

        NPC[A].Location.Height = (g_config.fix_npc_emerge_size) ? NPC[A]->THeight : 32;

        for(int bCheck = 1; bCheck <= 2; bCheck++)
        {
            // if(bCheck == 1)
            // {
            //     // fBlock = FirstBlock[(NPC[A].Location.X / 32) - 1];
            //     // lBlock = LastBlock[((NPC[A].Location.X + NPC[A].Location.Width) / 32.0) + 1];
            //     blockTileGet(NPC[A].Location, fBlock, lBlock);
            // }
            // else
            // {
                   // buggy, mentioned above, should be numBlock - numTempBlock + 1 -- ds-sloth
                   // it's not a problem here because the NPC is moved out of the way of the block
                   // during the first loop, so can't collide during the second loop.
            //     fBlock = numBlock - numTempBlock;
            //     lBlock = numBlock;
            // }

            auto collBlockSentinel = (bCheck == 1)
                ? treeFLBlockQuery(NPC[A].Location, SORTMODE_COMPAT)
                : treeTempBlockQuery(NPC[A].Location, SORTMODE_LOC);

            for(BlockRef_t block : collBlockSentinel)
            {
                int B = block;

                if(!Block[B].Invis && !(BlockIsSizable[Block[B].Type] && NPC[A].Location.Y > Block[B].Location.Y) && !Block[B].Hidden)
                {
                    if(CheckCollision(NPC[A].Location, Block[B].Location))
                    {
                        NPC[A].Location.Y = Block[B].Location.Y - NPC[A].Location.Height - 0.1;
                        break;
                    }
                }
            }
        }
    }
}

static inline void NPCEffectLogic_Encased(int A)
{
    bool still_encased = false;

    // Note: since SMBX64, this logic doesn't check for Hidden or Active, so an encased NPC will not escape encased mode properly in Battle Mode
    // Note 2: NPCID_BOSS_FRAGILE does not use the encased logic, it has its own specific logic to check for nearby NPCID_BOSS_CASE
    for(int B : treeNPCQuery(NPC[A].Location, SORTMODE_NONE))
    {
        if(NPC[B].Type == NPCID_BOSS_CASE)
        {
            if(CheckCollision(NPC[A].Location, NPC[B].Location))
            {
                still_encased = true;
                break;
            }
        }
    }

    if(!still_encased)
        NPC[A].Effect = NPCEFF_NORMAL;
}

static inline void NPCEffectLogic_DropItem(int A)
{
    // modern item drop
    if(NPC[A].Effect3 != 0)
    {
        const Player_t& p = Player[NPC[A].Effect3];
        const Location_t& pLoc = p.Location;
        Location_t& nLoc = NPC[A].Location;

        // Y logic
        vScreen_t& vscreen = vScreenByPlayer(NPC[A].Effect3);

        // put above player
        double target_X = pLoc.X + pLoc.Width / 2 - nLoc.Width / 2;
        double target_Y = pLoc.Y + pLoc.Height - 192;

        // anticipate player movement
        if(PlayerNormal(p))
        {
            target_X += pLoc.SpeedX;
            target_Y += pLoc.SpeedY;
        }

        // never allow item to go fully offscreen
        if(target_Y < -vscreen.Y - 8)
            target_Y = -vscreen.Y - 8;

        // perform movement
        double delta_X = target_X - nLoc.X;
        double delta_Y = target_Y - nLoc.Y;

        double move_X = delta_X / 8.0;
        double move_Y = delta_Y / 8.0;

        double dist_sq = (move_X * move_X + move_Y * move_Y);

        if(dist_sq > 0.0 && dist_sq < 128.0)
        {
            double dist = SDL_sqrt(dist_sq);
            move_X *= (8.0 * 1.4142135623730951) / dist;
            move_Y *= (8.0 * 1.4142135623730951) / dist;
        }

        if(std::abs(delta_Y) < std::abs(move_Y) || NPC[A].Special5 <= 66)
            nLoc.Y = target_Y;
        else
            nLoc.Y += move_Y;

        if(std::abs(delta_X) < std::abs(move_X) || NPC[A].Special5 <= 66)
            nLoc.X = target_X;
        else
            nLoc.X += move_X;

        // timer logic
        if(NPC[A].Special5 <= 66)
            NPC[A].Special5 -= 1;
        else if(nLoc.X == target_X && nLoc.Y == target_Y)
            NPC[A].Special5 = 66;

        // enter SMBX mode on timer expiration
        if(NPC[A].Special5 <= 0)
        {
            NPC[A].Effect3 = 0;
            NPC[A].Special5 = 0;
        }
    }
    else
    {
        NPC[A].Location.Y += 2.2;

        NPC[A].Effect2 += 1;
        if(NPC[A].Effect2 == 5)
            NPC[A].Effect2 = 1;
    }
}

static inline void NPCEffectLogic_Warp(int A)
{
    // NOTE: this code previously used Effect2 to store destination position, and now it uses SpecialX/Y
    if(NPC[A].Effect3 == 1)
    {
        NPC[A].Location.Y -= 1;
        if(NPC[A].Type == NPCID_PLATFORM_S1)
            NPC[A].Location.Y -= 1;

        if(NPC[A].Location.Y + NPC[A].Location.Height <= NPC[A].SpecialY)
        {
            NPC[A].Effect = NPCEFF_NORMAL;
            NPC[A].SpecialY = 0;
            NPC[A].Effect3 = 0;
        }
    }
    else if(NPC[A].Effect3 == 3)
    {
        NPC[A].Location.Y += 1;

        if(NPC[A].Type == NPCID_PLATFORM_S1)
            NPC[A].Location.Y += 1;

        if(NPC[A].Location.Y >= NPC[A].SpecialY)
        {
            NPC[A].Effect = NPCEFF_NORMAL;
            NPC[A].SpecialY = 0;
            NPC[A].Effect3 = 0;
        }
    }
    else if(NPC[A].Effect3 == 2)
    {
        if(NPC[A].Type == NPCID_POWER_S3 || NPC[A].Type == NPCID_LIFE_S3 || NPC[A].Type == NPCID_POISON || NPC[A].Type == NPCID_POWER_S1 || NPC[A].Type == NPCID_POWER_S4 || NPC[A].Type == NPCID_LIFE_S1 || NPC[A].Type == NPCID_LIFE_S4 || NPC[A].Type == NPCID_BRUTE_SQUISHED || NPC[A].Type == NPCID_BIG_MOLE)
            NPC[A].Location.X -= double(Physics.NPCMushroomSpeed);
        else if(NPC[A]->CanWalkOn)
            NPC[A].Location.X -= 1;
        else
            NPC[A].Location.X -= double(Physics.NPCWalkingSpeed);

        if(NPC[A].Location.X + NPC[A].Location.Width <= NPC[A].SpecialX)
        {
            NPC[A].Effect = NPCEFF_NORMAL;
            NPC[A].SpecialX = 0;
            NPC[A].Effect3 = 0;
        }
    }
    else if(NPC[A].Effect3 == 4)
    {
        if(NPC[A].Type == NPCID_POWER_S3 || NPC[A].Type == NPCID_LIFE_S3 || NPC[A].Type == NPCID_POISON || NPC[A].Type == NPCID_POWER_S1 || NPC[A].Type == NPCID_POWER_S4 || NPC[A].Type == NPCID_LIFE_S1 || NPC[A].Type == NPCID_LIFE_S4 || NPC[A].Type == NPCID_BRUTE_SQUISHED || NPC[A].Type == NPCID_BIG_MOLE)
            NPC[A].Location.X += double(Physics.NPCMushroomSpeed);
        else if(NPC[A]->CanWalkOn)
            NPC[A].Location.X += 1;
        else
            NPC[A].Location.X += double(Physics.NPCWalkingSpeed);

        if(NPC[A].Location.X >= NPC[A].SpecialX)
        {
            NPC[A].Effect = NPCEFF_NORMAL;
            NPC[A].SpecialX = 0;
            NPC[A].Effect3 = 0;
        }
    }

    NPCFrames(A);

    if(NPC[A].Effect == NPCEFF_NORMAL && NPC[A].Type != NPCID_ITEM_BURIED)
    {
        NPC[A].Layer = LAYER_SPAWNED_NPCS;
        syncLayers_NPC(A);
    }
}

static inline void NPCEffectLogic_PetTongue(int A)
{
    NPC[A].TimeLeft = 100;
    NPC[A].Effect3 -= 1;
    if(NPC[A].Effect3 <= 0)
    {
        NPC[A].Effect = NPCEFF_NORMAL;
        NPC[A].Effect2 = 0;
        NPC[A].Effect3 = 0;
    }
}

static inline void NPCEffectLogic_PetInside(int A)
{
    NPC[A].TimeLeft = 100;
    if(Player[NPC[A].Effect2].YoshiNPC != A)
    {
        NPC[A].Effect = NPCEFF_NORMAL;
        NPC[A].Effect2 = 0;
        NPC[A].Effect3 = 0;
    }
}

static inline void NPCEffectLogic_Waiting(int A)
{
    NPC[A].Effect2 -= 1;
    if(NPC[A].Effect2 <= 0)
    {
        NPC[A].Effect = NPCEFF_NORMAL;
        NPC[A].Effect2 = 0;
        NPC[A].Effect3 = 0;
    }
}


void NPCEffects(int A)
{
    if(NPC[A].Effect == NPCEFF_EMERGE_UP) // Bonus coming out of a block effect
        NPCEffectLogic_EmergeUp(A);
    else if(NPC[A].Effect == NPCEFF_ENCASED)
        NPCEffectLogic_Encased(A);
    else if(NPC[A].Effect == NPCEFF_DROP_ITEM) // Bonus item is falling from the players container effect
        NPCEffectLogic_DropItem(A);
    else if(NPC[A].Effect == NPCEFF_EMERGE_DOWN) // Bonus falling out of a block
        NPCEffectLogic_EmergeDown(A);
    else if(NPC[A].Effect == NPCEFF_WARP) // Warp Generator
        NPCEffectLogic_Warp(A);
    else if(NPC[A].Effect == NPCEFF_PET_TONGUE) // Grabbed by Yoshi
        NPCEffectLogic_PetTongue(A);
    else if(NPC[A].Effect == NPCEFF_PET_INSIDE) // Held by Yoshi
        NPCEffectLogic_PetInside(A);
    else if(NPC[A].Effect == NPCEFF_WAITING) // Holding Pattern
        NPCEffectLogic_Waiting(A);
}
