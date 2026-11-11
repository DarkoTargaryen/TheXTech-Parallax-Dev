// Experimental visual effects for the isolated shadow-experiment branch.
// Shadows now mask against the receiver's visible pixels (PNG alpha / GIF mask).
// It never modifies level graphics, object physics, or game data.
namespace VisualFXExperiment
{
    // Shared classification for emission and shadow-casting rules.
    static bool flameNPC(int type);

    struct Sprite
    {
        StdPicture* texture;
        int x, y, width, height;
        int src_x, src_y;
        // Physical contact bounds, separate from the animated sprite's pixels.
        bool is_actor;
        bool is_player;
        double feet_x, feet_y, feet_width, vertical_speed;
        double resting_speed;
        int standing_block_id;
        bool grabbing_item;
        bool casts_shadow;
    };

    static void addSprite(std::vector<Sprite>& out, StdPicture& texture,
                          int x, int y, int width, int height, int srcX, int srcY,
                          int screenWidth, int screenHeight,
                          const Location_t* actorLocation = nullptr,
                          double restingSpeed = 0.0,
                          int standingBlockId = 0,
                          bool isPlayer = false,
                          bool grabbingItem = false,
                          bool castsShadow = true)
    {
        if(width <= 0 || height <= 0 || x > screenWidth + 64 || y > screenHeight + 64 ||
           x + width < -64 || y + height < -64)
            return;
        Sprite sprite = {&texture, x, y, width, height, srcX, srcY,
                         actorLocation != nullptr, isPlayer, 0.0, 0.0, 0.0, 0.0, restingSpeed,
                         standingBlockId, grabbingItem, castsShadow};
        if(actorLocation)
        {
            sprite.feet_x = actorLocation->X;
            sprite.feet_y = actorLocation->Y + actorLocation->Height;
            sprite.feet_width = actorLocation->Width;
            sprite.vertical_speed = actorLocation->SpeedY;
        }
        out.push_back(sprite);
    }

    static void collectSprites(const int camX, const int camY,
                               const int screenWidth, const int screenHeight,
                               const std::vector<BlockRef_t>& mainBlocks,
                               std::vector<Sprite>& sprites)
    {
        sprites.clear();
        sprites.reserve(128);

        using CharacterTextures = RangeArr<StdPicture, 1, 10>;
        CharacterTextures* chars[] = {&GFXMario, &GFXLuigi, &GFXPeach, &GFXToad, &GFXLink};

        for(int A = 1; A <= numPlayers; ++A)
        {
            const Player_t& p = Player[A];
            if(p.Dead || p.TimeToLive || p.Immune2 || p.Character < 1 || p.Character > 5 ||
               p.State < 1 || p.State > 10 || p.Effect == PLREFF_WARP_PIPE ||
               p.Effect == PLREFF_WARP_DOOR || p.Effect == PLREFF_NO_COLLIDE ||
               p.Effect == PLREFF_WAITING || p.Effect == PLREFF_PET_INSIDE)
                continue;

            StdPicture& tx = (*chars[p.Character - 1])[p.State];
            const int playerX = camX + s_round2int(p.Location.X);
            const int playerY = camY + s_round2int(p.Location.Y);
            const int offX = pfrOffX(tx, p);
            const int offY = pfrOffY(tx, p);
            // Match DrawPlayer's visible upper-body crop when riding a shoe.
            // Otherwise a full standing sprite appears in the reflection and
            // casts a phantom shadow through the Kuribo's Shoe.
            int playerHeight = pfrH(tx, p);
            if(p.Mount == 1)
            {
                if(p.Duck)
                    playerHeight = 0;
                else
                {
                    const int peachCorrection = (p.Character == 3) ? -2 : 0;
                    playerHeight = s_round2int(p.Location.Height) - 26 - offY + peachCorrection;
                }
            }
            int riderX = playerX + offX;
            int riderY = playerY + offY + ((p.Mount == 1 && p.Character == 4 && p.State == 1) ? 6 : 0);
            if(p.Mount == 3)
                riderY += p.MountOffsetY;
            if(p.Mount == 2)
            {
                // The clown car draws its rider in gfx_update.cpp, cropped
                // to the upper body and offset relative to the vehicle.
                int yOffset = 30;
                if(p.State == 1)
                    yOffset = 24;
                else if(p.Character == 2)
                    yOffset = 38;
                else if(p.Character == 1)
                    yOffset = 36;
                riderX = playerX + offX -
                         s_round2int(Physics.PlayerWidth[p.Character][p.State] / 2.0) + 64;
                riderY = playerY + offY + p.MountOffsetY - yOffset;
                playerHeight = s_round2int(p.Location.Height) - 20 - p.MountOffsetY;
            }
            if(playerHeight > 0)
                addSprite(sprites, tx, riderX, riderY,
                          pfrW(tx, p), playerHeight, pfrX(tx, p), pfrY(tx, p),
                          screenWidth, screenHeight,
                          &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                          p.GrabTime > 0);

            if(p.Mount == 3 && p.MountType >= 1)
            {
                // Exact head/body positions and frames from DrawPlayer.
                const int yoshiX = camX + s_round2int(p.Location.X);
                const int yoshiY = camY + s_round2int(p.Location.Y);
                addSprite(sprites, GFXYoshiB[p.MountType],
                          yoshiX + p.YoshiBX, yoshiY + p.YoshiBY,
                          32, 32, 0, 32 * p.YoshiBFrame, screenWidth, screenHeight,
                          &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                          p.GrabTime > 0);
                addSprite(sprites, GFXYoshiT[p.MountType],
                          yoshiX + p.YoshiTX, yoshiY + p.YoshiTY,
                          32, 32, 0, 32 * p.YoshiTFrame, screenWidth, screenHeight,
                          &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                          p.GrabTime > 0);
                if(p.MountSpecial > 0)
                {
                    const int tongueX = camX + s_round2int(p.YoshiTongueX) -
                                        ((p.Direction == -1) ? p.YoshiTongueLength : 0) - 1;
                    const int tongueY = camY + s_round2int(p.YoshiTongue.Y);
                    addSprite(sprites, GFX.Tongue[2], tongueX, tongueY,
                              p.YoshiTongueLength + 2, 16, 0, 0,
                              screenWidth, screenHeight,
                              &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                              p.GrabTime > 0);
                    addSprite(sprites, GFX.Tongue[1],
                              camX + s_round2int(p.YoshiTongue.X), tongueY,
                              16, 16, 0, 16 * ((p.Direction == 1) ? 0 : 1),
                              screenWidth, screenHeight,
                              &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                              p.GrabTime > 0);
                }
                if(p.YoshiBlue)
                    addSprite(sprites, GFX.YoshiWings,
                              yoshiX + p.YoshiBX + ((p.Direction == 1) ? -12 : 12),
                              yoshiY + p.YoshiBY - 16,
                              32, 32, 0, 32 * p.YoshiWingsFrame,
                              screenWidth, screenHeight,
                              &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                              p.GrabTime > 0);
            }
            if(p.Mount == 2)
            {
                addSprite(sprites, GFX.Mount[p.Mount],
                          playerX + s_round2int(p.Location.Width) / 2 - 64,
                          playerY + s_round2int(p.Location.Height) - 128,
                          128, 128, 0, 128 * p.MountFrame,
                          screenWidth, screenHeight,
                          &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                          p.GrabTime > 0);
            }
            if(p.Mount == 1 && p.MountType >= 1 && p.MountType <= 3)
            {
                // The boot is a separate 32x32 sprite, drawn at these exact
                // coordinates and animation frame in gfx_draw_player.cpp.
                addSprite(sprites, GFX.Boot[p.MountType],
                          playerX + s_round2int(p.Location.Width) / 2 - 16,
                          playerY + s_round2int(p.Location.Height) - 30,
                          32, 32, 0, 32 * p.MountFrame,
                          screenWidth, screenHeight,
                          &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                          p.GrabTime > 0);
            }
            if((p.Mount == 1 && p.MountType == 3) || p.Effect == PLREFF_COOP_WINGS)
                addSprite(sprites, GFX.YoshiWings,
                          playerX + ((p.Direction == 1) ? -24 : 16),
                          playerY + s_round2int(p.Location.Height) - 40,
                          32, 32, 0, 32 * p.YoshiWingsFrame,
                          screenWidth, screenHeight,
                          &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                          p.GrabTime > 0);
        }

        // NPCs include vines and enemies: use the engine's current animation frame.
        for(NPCRef_t n : treeNPCQuery(-camX - 64, -camY - 64,
                                      -camX + screenWidth + 64,
                                      -camY + screenHeight + 64, SORTMODE_NONE))
        {
            const NPC_t& npc = *n;
            if(npc.Hidden || npc.Type <= 0 || npc.Type == NPCID_ITEM_BURIED ||
               npc.Killed || !npc.Active)
                continue;
            const int w = npc->WidthGFX > 0 ? npc->WidthGFX : s_round2int(npc.Location.Width);
            const int h = npc->HeightGFX > 0 ? npc->HeightGFX : s_round2int(npc.Location.Height);
            const int x = camX + s_round2int(npc.Location.X) + npc->FrameOffsetX +
                (s_round2int(npc.Location.Width) - w) / 2;
            const int y = camY + s_round2int(npc.Location.Y) + npc->FrameOffsetY +
                s_round2int(npc.Location.Height) - h;
            addSprite(sprites, GFXNPC[npc.Type], x, y, w, h,
                      0, npc.Frame * h, screenWidth, screenHeight,
                      &npc.Location, Physics.NPCGravityReal, npc.shadowStandingBlock,
                      false, false,
                      !flameNPC(npc.Type) && npc.Type != NPCID_WALL_SPARK);
        }

        // Ordinary blocks cast shadows but do not receive them.
        for(Block_t& b : mainBlocks)
        {
            if(b.Hidden || b.Invis || b.Type <= 0)
                continue;
            addSprite(sprites, GFXBlockBMP[b.Type],
                      camX + s_round2int(b.Location.X),
                      camY + s_round2int(b.Location.Y),
                      s_round2int(b.Location.Width), s_round2int(b.Location.Height),
                      0, BlockFrame[b.Type] * 32, screenWidth, screenHeight);
        }
    }

    // Clip sprite source/destination to one rectangular receiver.
    // NOTE: this does not intersect with the receiver's PNG alpha yet.
    static void drawClipped(const Sprite& sprite,
                            int rx, int ry, int rw, int rh,
                            bool reflected, XTColor tint)
    {
        const int x = sprite.x + (reflected ? 0 : 8);
        const int y = sprite.y + (reflected ? 0 : 8);
        const int l = std::max(rx, x), t = std::max(ry, y);
        const int r = std::min(rx + rw, x + sprite.width);
        const int b = std::min(ry + rh, y + sprite.height);
        if(r <= l || b <= t)
            return;

        const int w = r - l, h = b - t;
        const int sx = sprite.src_x + l - x;
        const int sy = sprite.src_y + (reflected ? sprite.height - (t - y) - h : t - y);
        XRender::renderTextureScaleEx(l, t, w, h, *sprite.texture,
                                     sx, sy, w, h, 0.0, nullptr,
                                     reflected ? X_FLIP_VERTICAL : X_FLIP_NONE, tint);
    }

    // Draw a caster silhouette where BOTH the caster and receiver are opaque.
    // The backend handles PNG alpha plus legacy GIF bitmasks automatically.
    static void drawReceiverShadow(const Sprite& sprite,
                                   int rx, int ry, int rw, int rh,
                                   StdPicture& receiver, int receiverSrcX, int receiverSrcY,
                                   XTColor tint)
    {
        if(!sprite.casts_shadow)
            return;
        const int sx = sprite.x + 8;
        const int sy = sprite.y + 8;
        const int l = std::max(rx, sx), t = std::max(ry, sy);
        const int r = std::min(rx + rw, sx + sprite.width);
        const int b = std::min(ry + rh, sy + sprite.height);
        if(r <= l || b <= t)
            return;
        XRender::renderMaskedShadow(l, t, r - l, b - t,
                                    *sprite.texture, sprite.src_x + l - sx,
                                    sprite.src_y + t - sy,
                                    receiver, receiverSrcX + l - rx,
                                    receiverSrcY + t - ry, tint);
    }

    // Draw the receiving tile except for the screen rectangle hidden behind
    // an actor's supporting front resizable. We split the tile into four
    // non-overlapping strips; the existing shader still handles PNG/GIF alpha.
    // Receiver source offsets must follow the clipped destination exactly.
    static void drawReceiverShadowOutsideRect(const Sprite& sprite,
                                              int rx, int ry, int rw, int rh,
                                              StdPicture& receiver, int receiverSrcX,
                                              int receiverSrcY, XTColor tint,
                                              int coverX, int coverY,
                                              int coverW, int coverH)
    {
        const int x0 = std::max(rx, coverX);
        const int y0 = std::max(ry, coverY);
        const int x1 = std::min(rx + rw, coverX + coverW);
        const int y1 = std::min(ry + rh, coverY + coverH);
        if(x0 >= x1 || y0 >= y1)
        {
            drawReceiverShadow(sprite, rx, ry, rw, rh, receiver,
                               receiverSrcX, receiverSrcY, tint);
            return;
        }

        // Upper and lower strips cover the tile's full width.
        if(y0 > ry)
            drawReceiverShadow(sprite, rx, ry, rw, y0 - ry, receiver,
                               receiverSrcX, receiverSrcY, tint);
        if(y1 < ry + rh)
            drawReceiverShadow(sprite, rx, y1, rw, ry + rh - y1, receiver,
                               receiverSrcX, receiverSrcY + (y1 - ry), tint);

        // Left and right strips are limited to the covered vertical span.
        if(x0 > rx)
            drawReceiverShadow(sprite, rx, y0, x0 - rx, y1 - y0, receiver,
                               receiverSrcX, receiverSrcY + (y0 - ry), tint);
        if(x1 < rx + rw)
            drawReceiverShadow(sprite, x1, y0, rx + rw - x1, y1 - y0, receiver,
                               receiverSrcX + (x1 - rx), receiverSrcY + (y0 - ry), tint);
    }

    static void drawShadows(const std::vector<Sprite>& sprites,
                            int camX, int camY,
                            const std::vector<BaseRef_t>& bgos,
                            const std::vector<BlockRef_t>& sizable)
    {
        const XTColor shadow(16, 20, 29, 100);

        // Prefer the support block selected by the engine's real player collision.
        // Recompute from live contacts for NPCs, which don't store a floorBlock.
        // Never remember the previously touched block after leaving it.
        std::vector<const Block_t*> supportingBlock(sprites.size(), nullptr);
        for(size_t i = 0; i < sprites.size(); ++i)
        {
            const Sprite& sprite = sprites[i];
            if(!sprite.casts_shadow || !sprite.is_actor || sprite.feet_width <= 0.0)
                continue;

            if(sprite.standing_block_id > 0 && sprite.standing_block_id <= numBlock)
            {
                const Block_t& support = Block[sprite.standing_block_id];
                const double overlap = std::min(sprite.feet_x + sprite.feet_width,
                                                support.Location.X + support.Location.Width)
                                     - std::max(sprite.feet_x, support.Location.X);
                // Confirm that the cached grab support is still under the actor.
                // Most frames this ID was freshly selected by the physics engine.
                const double delta = sprite.feet_y - support.Location.Y;
                if(support.Type > 0 && !support.Hidden && !support.Invis &&
                   BlockIsSizable[support.Type] && overlap > 0.0 &&
                   // Real-time collision contact is authoritative; only a
                   // cached pickup contact needs a positional sanity check.
                   (!sprite.grabbing_item || (delta >= -4.0 && delta <= 32.0)))
                    supportingBlock[i] = &support;
            }

            if(supportingBlock[i])
                continue;

            // During an NPC 91 pickup, the actor may be animated a little below
            // the surface with no floor collision. Allow that specific animation
            // to find its support, but never infer player support while airborne.
            if(sprite.is_player && !sprite.grabbing_item)
                continue;

            if(sprite.is_player && sprite.grabbing_item)
            {
                double closest = 33.0;
                for(Block_t& candidate : sizable)
                {
                    if(candidate.Hidden || candidate.Invis || candidate.Type <= 0)
                        continue;
                    const double delta = sprite.feet_y - candidate.Location.Y;
                    const double overlap = std::min(sprite.feet_x + sprite.feet_width,
                                                    candidate.Location.X + candidate.Location.Width)
                                         - std::max(sprite.feet_x, candidate.Location.X);
                    if(delta < -4.0 || delta > 32.0 || overlap <= 0.0)
                        continue;
                    if(!supportingBlock[i] || std::abs(delta) < closest)
                    {
                        supportingBlock[i] = &candidate;
                        closest = std::abs(delta);
                    }
                }
                continue;
            }

            // NPCs: near-exact contact with a sizable's upper surface and no
            // upward movement. Allow grounded NPC velocity to be 0 or gravity.
            if(sprite.vertical_speed < -0.05 ||
               sprite.vertical_speed > std::max(1.0, sprite.resting_speed + 0.25))
                continue;

            double nearest = 2.5;
            double widest = 0.0;
            for(Block_t& candidate : sizable)
            {
                if(candidate.Hidden || candidate.Invis || candidate.Type <= 0)
                    continue;
                const double distance = std::abs(sprite.feet_y - candidate.Location.Y);
                const double overlap = std::min(sprite.feet_x + sprite.feet_width,
                                                candidate.Location.X + candidate.Location.Width)
                                     - std::max(sprite.feet_x, candidate.Location.X);
                if(distance > 2.5 || overlap <= 0.0)
                    continue;
                if(!supportingBlock[i] || distance < nearest - 0.01 ||
                   (std::abs(distance - nearest) <= 0.01 && overlap > widest))
                {
                    supportingBlock[i] = &candidate;
                    nearest = distance;
                    widest = overlap;
                }
            }
        }

        // BGOs are independent receivers, so they still receive actor shadows.
        // BGO receivers only: water/quicksand are NEVER drawn as shadow targets.
        for(BaseRef_t ref : bgos)
        {
            const Background_t& bgo = Background[static_cast<int>(ref)];
            if(bgo.Hidden || bgo.Type <= 0)
                continue;
            const int x = camX + s_round2int(bgo.Location.X);
            const int y = camY + s_round2int(bgo.Location.Y);
            const int w = s_round2int(bgo.Location.Width);
            const int h = s_round2int(bgo.Location.Height);
            StdPicture& receiver = GFXBackgroundBMP[bgo.Type];
            const int srcY = BackgroundHeight[bgo.Type] * BackgroundFrame[bgo.Type];
            // A BGO sits behind sizable blocks. Subtract the visible sizable
            // rectangles before submitting its shadow; otherwise the shadow
            // pass (drawn later) paints BGO silhouettes over foreground floors.
            // Clip only the caster/receiver overlap to avoid splitting entire
            // large BGOs for every sizable in the scene.
            struct ShadowPiece
            {
                int x, y, w, h;
            };
            for(const Sprite& sprite : sprites)
            {
                const int left = std::max(x, sprite.x + 8);
                const int top = std::max(y, sprite.y + 8);
                const int right = std::min(x + w, sprite.x + 8 + sprite.width);
                const int bottom = std::min(y + h, sprite.y + 8 + sprite.height);
                if(right <= left || bottom <= top)
                    continue;

                std::vector<ShadowPiece> pieces(1, {left, top, right - left, bottom - top});
                for(Block_t& occluder : sizable)
                {
                    if(occluder.Hidden || occluder.Invis || occluder.Type <= 0)
                        continue;
                    const int ox = camX + s_round2int(occluder.Location.X);
                    const int oy = camY + s_round2int(occluder.Location.Y);
                    const int ow = s_round2int(occluder.Location.Width);
                    const int oh = s_round2int(occluder.Location.Height);
                    if(ow <= 0 || oh <= 0 || ox >= right || oy >= bottom ||
                       ox + ow <= left || oy + oh <= top)
                        continue;

                    std::vector<ShadowPiece> visible;
                    visible.reserve(pieces.size() * 2);
                    for(const ShadowPiece& p : pieces)
                    {
                        const int l = std::max(p.x, ox);
                        const int t = std::max(p.y, oy);
                        const int r = std::min(p.x + p.w, ox + ow);
                        const int b = std::min(p.y + p.h, oy + oh);
                        if(l >= r || t >= b)
                        {
                            visible.push_back(p);
                            continue;
                        }
                        // Four non-overlapping strips around the foreground
                        // block. Receiver texture offsets are recovered below.
                        if(t > p.y)
                            visible.push_back({p.x, p.y, p.w, t - p.y});
                        if(b < p.y + p.h)
                            visible.push_back({p.x, b, p.w, p.y + p.h - b});
                        if(l > p.x)
                            visible.push_back({p.x, t, l - p.x, b - t});
                        if(r < p.x + p.w)
                            visible.push_back({r, t, p.x + p.w - r, b - t});
                    }
                    pieces.swap(visible);
                    if(pieces.empty())
                        break;
                }
                for(const ShadowPiece& p : pieces)
                    drawReceiverShadow(sprite, p.x, p.y, p.w, p.h,
                                       receiver, p.x - x, srcY + p.y - y, shadow);
            }
        }

        for(Block_t& block : sizable)
        {
            if(block.Hidden || block.Invis || block.Type <= 0)
                continue;
            const int x = camX + s_round2int(block.Location.X);
            const int y = camY + s_round2int(block.Location.Y);
            const int w = s_round2int(block.Location.Width);
            const int h = s_round2int(block.Location.Height);
            StdPicture& receiver = GFXBlockBMP[block.Type];
            // Sizable blocks use a 3x3 texture: source cells are 0, 32 and 64.
            for(int tileY = 0; tileY < h; tileY += 32)
            {
                const int tileH = std::min(32, h - tileY);
                const int srcY = tileY == 0 ? 0 : (tileY + 32 >= h ? 64 : 32);
                for(int tileX = 0; tileX < w; tileX += 32)
                {
                    const int tileW = std::min(32, w - tileX);
                    const int srcX = tileX == 0 ? 0 : (tileX + 32 >= w ? 64 : 32);
                    for(size_t i = 0; i < sprites.size(); ++i)
                    {
                        // Only suppress the shadow on the ACTUAL supporting block.
                        // All other sizeable blocks remain eligible receivers.
                        const Block_t* support = supportingBlock[i];
                        if(support == &block)
                            continue;
                        // Same-height touching resizables form one walking
                        // floor, so suppress the grounded actor on both.
                        if(support && sprites[i].is_actor)
                        {
                            const bool sameLevelAdjacent =
                                std::abs(support->Location.Y - block.Location.Y) <= 1.5 &&
                                support->Location.X <= block.Location.X + block.Location.Width + 1.0 &&
                                block.Location.X <= support->Location.X + support->Location.Width + 1.0;
                            if(sameLevelAdjacent)
                                continue;

                            // The tall rear resizable still receives shadows
                            // above and beside the smaller supporting block.
                            // Only subtract pixels the foreground block covers.
                            if(support->Location.Y >= block.Location.Y)
                            {
                                const int coverX = camX + s_round2int(support->Location.X);
                                const int coverY = camY + s_round2int(support->Location.Y);
                                const int coverW = s_round2int(support->Location.Width);
                                const int coverH = s_round2int(support->Location.Height);
                                drawReceiverShadowOutsideRect(sprites[i], x + tileX, y + tileY,
                                                              tileW, tileH, receiver,
                                                              srcX, srcY, shadow,
                                                              coverX, coverY, coverW, coverH);
                                continue;
                            }
                        }
                        drawReceiverShadow(sprites[i], x + tileX, y + tileY, tileW, tileH,
                                           receiver, srcX, srcY, shadow);
                    }
                }
            }
        }
    }

    // Local light sources are combined into a single screen-space field.
    // Max blending (rather than rendering one translucent halo per object)
    // prevents double-bright tiles and seams where lava blocks overlap.
    struct EmissiveLight
    {
        int x0, x1, y, radius, strength;
        bool electric;
    };

    static bool isLavaBlock(int type)
    {
        // Vanilla SMBX / TheXTech lava and burning tiles, including slopes.
        // Restrict to these IDs because BlockKills also covers non-lava hazards.
        return type == 30 || type == 371 ||
               (type >= 404 && type <= 406) || type == 420 ||
               (type >= 459 && type <= 487);
    }

    static bool flameNPC(int type)
    {
        switch(type)
        {
        case NPCID_LAVABUBBLE:
        case NPCID_PLR_FIREBALL:
        case NPCID_STATUE_FIRE:
        case NPCID_VILLAIN_FIRE:
        case NPCID_PET_FIRE:
        case NPCID_LAVA_MONSTER:
        case NPCID_PLANT_FIRE:
        case NPCID_FIRE_DISK:
        case NPCID_FIRE_CHAIN:
        case NPCID_QUAD_BALL:
        case NPCID_FIRE_BOSS_FIRE:
            return true;
        default:
            return false;
        }
    }

    static bool flameEffect(int type)
    {
        return type == EFFID_BIG_FIREBALL_TAIL ||
               type == EFFID_LAVA_SPLASH ||
               type == EFFID_PLR_FIREBALL_TRAIL ||
               type == EFFID_LAVA_MONSTER_LOOK ||
               type == EFFID_FIRE_DISK_DIE;
    }

    static void drawEmissiveLighting(int camX, int camY,
                                     int screenWidth, int screenHeight,
                                     const std::vector<BlockRef_t>& deadlyBlocks)
    {
        if(screenWidth <= 0 || screenHeight <= 0)
            return;

        constexpr int cell = 8;
        const int cols = (screenWidth + cell - 1) / cell;
        const int rows = (screenHeight + cell - 1) / cell;
        std::vector<EmissiveLight> lights;
        lights.reserve(128);

        // Visible fire projectiles and hazards. All positions follow the
        // actual active NPCs, including fired plant/Bowser/Yoshi projectiles.
        for(NPCRef_t n : treeNPCQuery(-camX - 96, -camY - 96,
                                      -camX + screenWidth + 96,
                                      -camY + screenHeight + 96, SORTMODE_NONE))
        {
            const NPC_t& npc = *n;
            if(npc.Hidden || npc.Killed || !npc.Active || npc.Type <= 0)
                continue;
            const bool electric = (npc.Type == NPCID_WALL_SPARK);
            if(!flameNPC(npc.Type) && !electric)
                continue;
            const int x = camX + s_round2int(npc.Location.X + npc.Location.Width / 2);
            const int y = camY + s_round2int(npc.Location.Y + npc.Location.Height / 2);
            const int size = s_round2int(std::max(npc.Location.Width, npc.Location.Height));
            const int radius = std::min(100, std::max(32, size + 22));
            const int power = electric ? 39 : (npc.Type == NPCID_LAVA_MONSTER ? 56 : 48);
            lights.push_back({x, x, y, radius, power, electric});
        }

        // Brief flame tails and lava splashes emit light during their lifetime.
        for(int i = 1; i <= numEffects; ++i)
        {
            const auto& e = Effect[i];
            if(!flameEffect(e.Type) && e.Type != EFFID_WALL_SPARK_DIE)
                continue;
            const int x = camX + s_round2int(e.Location.X + e.Location.Width / 2);
            const int y = camY + s_round2int(e.Location.Y + e.Location.Height / 2);
            if(x < -80 || y < -80 || x > screenWidth + 80 || y > screenHeight + 80)
                continue;
            const bool electric = e.Type == EFFID_WALL_SPARK_DIE;
            lights.push_back({x, x, y, 29, 26, electric});
        }

        // Compute only the exposed upper envelope of lava. At every column,
        // stacked tiles share the highest visible surface, regardless of how
        // many tiles overlap beneath it. Adjacent equal-height columns merge
        // into one continuous light bar with a shared flicker phase.
        std::vector<int> lavaTop(cols, screenHeight + 1000);
        for(Block_t& b : deadlyBlocks)
        {
            if(b.Hidden || b.Invis || b.Type <= 0 || !isLavaBlock(b.Type))
                continue;
            const int left = camX + s_round2int(b.Location.X);
            const int right = camX + s_round2int(b.Location.X + b.Location.Width);
            const int top = camY + s_round2int(b.Location.Y);
            if(right <= 0 || left >= screenWidth || top >= screenHeight + 72)
                continue;
            const int first = std::max(0, left / cell);
            const int last = std::min(cols, (right + cell - 1) / cell);
            for(int c = first; c < last; ++c)
            {
                const int sampleX = c * cell + cell / 2;
                if(sampleX >= left && sampleX < right)
                    lavaTop[c] = std::min(lavaTop[c], top);
            }
        }

        const int lavaFlicker = s_round2int(std::sin(CommonFrame * 0.065) * 3.0);
        for(int c = 0; c < cols;)
        {
            if(lavaTop[c] > screenHeight + 80)
            {
                ++c;
                continue;
            }
            const int top = lavaTop[c];
            const int first = c;
            while(c < cols && std::abs(lavaTop[c] - top) <= 2)
                ++c;
            lights.push_back({first * cell, std::min(screenWidth, c * cell),
                              top, 66, 39 + lavaFlicker, false});
        }

        // One output per 8x8 game-space pixel cell. MAX composition prevents
        // light stacking from producing seams or blown-out yellow areas.
        std::vector<uint8_t> warm(cols * rows, 0);
        std::vector<uint8_t> blue(cols * rows, 0);
        for(const EmissiveLight& l : lights)
        {
            const int startC = std::max(0, (l.x0 - l.radius) / cell);
            const int endC = std::min(cols, (l.x1 + l.radius + cell - 1) / cell);
            const int startR = std::max(0, (l.y - l.radius) / cell);
            const int endR = std::min(rows, (l.y + l.radius + cell - 1) / cell);
            for(int ry = startR; ry < endR; ++ry)
            {
                const int dy = ry * cell + cell / 2 - l.y;
                for(int cx = startC; cx < endC; ++cx)
                {
                    const int sx = cx * cell + cell / 2;
                    const int dx = sx < l.x0 ? l.x0 - sx : (sx > l.x1 ? sx - l.x1 : 0);
                    const int distSquared = dx * dx + dy * dy;
                    if(distSquared >= l.radius * l.radius)
                        continue;
                    const double falloff = 1.0 - std::sqrt(double(distSquared)) / l.radius;
                    const int power = s_round2int(l.strength * falloff * falloff);
                    if(power <= 0)
                        continue;
                    uint8_t& out = (l.electric ? blue : warm)[ry * cols + cx];
                    out = std::max(out, static_cast<uint8_t>(std::min(65, power)));
                }
            }
        }

        // Coalesce cells of similar strength into horizontal runs to keep
        // draw calls modest. A small global cap also limits mixed fire+lava.
        for(int y = 0; y < rows; ++y)
        {
            for(int x = 0; x < cols;)
            {
                const int idx = y * cols + x;
                const bool isBlue = blue[idx] > warm[idx];
                const uint8_t intensity = isBlue ? blue[idx] : warm[idx];
                const int quantized = (intensity + 3) / 6;
                if(quantized == 0)
                {
                    ++x;
                    continue;
                }
                const int first = x++;
                while(x < cols)
                {
                    const int next = y * cols + x;
                    const bool nextBlue = blue[next] > warm[next];
                    const uint8_t nextStrength = nextBlue ? blue[next] : warm[next];
                    if(nextBlue != isBlue || (nextStrength + 3) / 6 != quantized)
                        break;
                    ++x;
                }
                const int opacity = std::min(65, quantized * 6);
                XRender::renderRect(first * cell, y * cell,
                                    std::min(screenWidth, x * cell) - first * cell,
                                    std::min(cell, screenHeight - y * cell),
                                    isBlue ? XTColor(145, 195, 255, opacity)
                                           : XTColor(255, 177, 93, opacity), true);
            }
        }
    }

    static void drawReflections(const std::vector<Sprite>& sprites,
                                int camX, int camY, int screenWidth, int screenHeight)
    {
        for(int n : treeWaterQuery(-camX, -camY,
                                   -camX + screenWidth, -camY + screenHeight,
                                   SORTMODE_ID))
        {
            const Water_t& water = Water[n];
            if(water.Hidden || water.Quicksand)
                continue;
            const int wx = camX + s_round2int(water.Location.X);
            const int wy = camY + s_round2int(water.Location.Y);
            const int ww = s_round2int(water.Location.Width);
            const int wh = s_round2int(water.Location.Height);

            for(const Sprite& sprite : sprites)
            {
                int groupTop = sprite.y;
                int groupBottom = sprite.y + sprite.height;
                if(sprite.is_actor)
                {
                    // Mount/rider components share a single submersion curve.
                    for(const Sprite& part : sprites)
                    {
                        if(!part.is_actor || part.is_player != sprite.is_player ||
                           part.feet_x != sprite.feet_x || part.feet_y != sprite.feet_y ||
                           part.feet_width != sprite.feet_width)
                            continue;
                        groupTop = std::min(groupTop, part.y);
                        groupBottom = std::max(groupBottom, part.y + part.height);
                    }
                }

                // Preserve the whole mirrored silhouette instead of cropping
                // it abruptly at the waterline. A smooth opacity transition
                // handles entry/exit and hides portions behind the real actor.
                const double totalHeight = std::max(1, groupBottom - groupTop);
                const double visible = std::max(0.0, std::min(1.0,
                    (wy - groupTop) / totalHeight));
                if(visible <= 0.0)
                    continue;
                const double submersionFade = visible * visible * (3.0 - 2.0 * visible);

                Sprite mirrored = sprite;
                mirrored.y = 2 * wy - (sprite.y + sprite.height);
                const int startY = std::max(wy, mirrored.y);
                const int endY = std::min(wy + wh, mirrored.y + mirrored.height);
                if(startY >= endY)
                    continue;

                for(int stripY = startY; stripY < endY; stripY += 4)
                {
                    const int stripH = std::min(4, endY - stripY);
                    const int depth = stripY - wy;
                    const double depthFade = std::max(0.0, 1.0 - depth / 112.0);
                    if(depthFade <= 0.0)
                        break;

                    // Fade across the actor/reflection intersection instead
                    // of imposing the previous hard 'start at groupBottom'.
                    // This keeps a natural reflection when jumping into water.
                    double overlapFade = 1.0;
                    if(sprite.is_actor && groupBottom > wy)
                    {
                        const double blend = std::max(0.0, std::min(1.0,
                            (stripY - groupBottom + 12.0) / 28.0));
                        overlapFade = blend * blend * (3.0 - 2.0 * blend);
                    }
                    const int opacity = s_round2int(110 * depthFade *
                                                     submersionFade * overlapFade);
                    if(opacity <= 0)
                        continue;
                    const double phase = depth * 0.19 + CommonFrame * 0.055;
                    const int shift = s_round2int(std::sin(phase) * 2.0);
                    Sprite rippled = mirrored;
                    rippled.x += shift;
                    drawClipped(rippled, wx, stripY, ww, stripH, true,
                                XTColor(155, 195, 220, static_cast<uint8_t>(opacity)));
                }
            }
        }
    }
}
