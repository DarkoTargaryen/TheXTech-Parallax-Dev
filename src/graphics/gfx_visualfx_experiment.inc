// Experimental visual effects for the isolated shadow-experiment branch.
// Shadows now mask against the receiver's visible pixels (PNG alpha / GIF mask).
// It never modifies level graphics, object physics, or game data.
namespace VisualFXExperiment
{
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
    };

    static void addSprite(std::vector<Sprite>& out, StdPicture& texture,
                          int x, int y, int width, int height, int srcX, int srcY,
                          int screenWidth, int screenHeight,
                          const Location_t* actorLocation = nullptr,
                          double restingSpeed = 0.0,
                          int standingBlockId = 0,
                          bool isPlayer = false,
                          bool grabbingItem = false)
    {
        if(width <= 0 || height <= 0 || x > screenWidth + 64 || y > screenHeight + 64 ||
           x + width < -64 || y + height < -64)
            return;
        Sprite sprite = {&texture, x, y, width, height, srcX, srcY,
                         actorLocation != nullptr, isPlayer, 0.0, 0.0, 0.0, 0.0, restingSpeed,
                         standingBlockId, grabbingItem};
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
            if(playerHeight > 0)
                addSprite(sprites, tx,
                          playerX + offX,
                          playerY + offY + ((p.Mount == 1 && p.Character == 4 && p.State == 1) ? 6 : 0),
                          pfrW(tx, p), playerHeight, pfrX(tx, p), pfrY(tx, p),
                          screenWidth, screenHeight,
                          &p.Location, Physics.PlayerGravity, p.shadowStandingBlock, true,
                          p.GrabTime > 0);

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
                      &npc.Location, Physics.NPCGravityReal, npc.shadowStandingBlock);
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
            if(!sprite.is_actor || sprite.feet_width <= 0.0)
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
                Sprite mirrored = sprite;
                mirrored.y = 2 * wy - (sprite.y + sprite.height);
                const int startY = std::max(wy, mirrored.y);
                const int endY = std::min(wy + wh, mirrored.y + mirrored.height);
                if(startY >= endY)
                    continue;

                // Reflected water image: subtle moving horizontal distortion
                // and depth-dependent alpha. The original caster stays intact.
                // Four-pixel strips avoid disrupting pixel-art filtering.
                for(int stripY = startY; stripY < endY; stripY += 4)
                {
                    const int stripH = std::min(4, endY - stripY);
                    const int depth = stripY - wy;
                    const double fade = std::max(0.0, 1.0 - depth / 112.0);
                    if(fade <= 0.0)
                        break;
                    const double phase = depth * 0.19 + CommonFrame * 0.055;
                    const int shift = s_round2int(std::sin(phase) * 2.0);
                    Sprite rippled = mirrored;
                    rippled.x += shift;
                    drawClipped(rippled, wx, stripY, ww, stripH, true,
                                XTColor(155, 195, 220, static_cast<uint8_t>(110 * fade)));
                }
            }
        }
    }
}
