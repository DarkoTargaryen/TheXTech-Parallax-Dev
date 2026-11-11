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
        double feet_x, feet_y, feet_width, vertical_speed;
        double resting_speed;
    };

    static void addSprite(std::vector<Sprite>& out, StdPicture& texture,
                          int x, int y, int width, int height, int srcX, int srcY,
                          int screenWidth, int screenHeight,
                          const Location_t* actorLocation = nullptr,
                          double restingSpeed = 0.0)
    {
        if(width <= 0 || height <= 0 || x > screenWidth + 64 || y > screenHeight + 64 ||
           x + width < -64 || y + height < -64)
            return;
        Sprite sprite = {&texture, x, y, width, height, srcX, srcY,
                         actorLocation != nullptr, 0.0, 0.0, 0.0, 0.0, restingSpeed};
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
               p.Effect == PLREFF_WAITING || p.Effect == PLREFF_PET_INSIDE)
                continue;

            StdPicture& tx = (*chars[p.Character - 1])[p.State];
            addSprite(sprites, tx,
                      camX + s_round2int(p.Location.X) + pfrOffX(tx, p),
                      camY + s_round2int(p.Location.Y) + pfrOffY(tx, p),
                      pfrW(tx, p), pfrH(tx, p), pfrX(tx, p), pfrY(tx, p),
                      screenWidth, screenHeight,
                      &p.Location, Physics.PlayerGravity);
        }

        // NPCs include vines and enemies: use the engine's current animation frame.
        for(NPCRef_t n : treeNPCQuery(-camX - 64, -camY - 64,
                                      -camX + screenWidth + 64,
                                      -camY + screenHeight + 64, SORTMODE_NONE))
        {
            const NPC_t& npc = *n;
            if(npc.Hidden || npc.Type <= 0 || npc.Killed || !npc.Active)
                continue;
            const int w = npc->WidthGFX > 0 ? npc->WidthGFX : s_round2int(npc.Location.Width);
            const int h = npc->HeightGFX > 0 ? npc->HeightGFX : s_round2int(npc.Location.Height);
            const int x = camX + s_round2int(npc.Location.X) + npc->FrameOffsetX +
                (s_round2int(npc.Location.Width) - w) / 2;
            const int y = camY + s_round2int(npc.Location.Y) + npc->FrameOffsetY +
                s_round2int(npc.Location.Height) - h;
            addSprite(sprites, GFXNPC[npc.Type], x, y, w, h,
                      0, npc.Frame * h, screenWidth, screenHeight,
                      &npc.Location, Physics.NPCGravityReal);
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

    static void drawShadows(const std::vector<Sprite>& sprites,
                            int camX, int camY,
                            const std::vector<BaseRef_t>& bgos,
                            const std::vector<BlockRef_t>& sizable)
    {
        const XTColor shadow(16, 20, 29, 100);

        // Find the *current* supporting sizeable block per actor every frame.
        // Do not retain previous supports: on jump/fall, shadows return immediately.
        // Compare physical feet (not sprite art) with the upper surface of the block.
        // A character standing on an SMBX platform can have gravity-sized SpeedY.
        std::vector<const Block_t*> supportingBlock(sprites.size(), nullptr);
        for(size_t i = 0; i < sprites.size(); ++i)
        {
            const Sprite& sprite = sprites[i];
            if(!sprite.is_actor || sprite.feet_width <= 0.0 ||
               sprite.vertical_speed < -0.1 ||
               sprite.vertical_speed > std::max(0.75, sprite.resting_speed + 0.25))
                continue;

            double bestDistance = 1.5;
            double bestOverlap = 0.0;
            for(Block_t& candidate : sizable)
            {
                if(candidate.Hidden || candidate.Invis || candidate.Type <= 0)
                    continue;
                const double yDistance = std::abs(sprite.feet_y - candidate.Location.Y);
                const double overlap = std::min(sprite.feet_x + sprite.feet_width,
                                                candidate.Location.X + candidate.Location.Width)
                                     - std::max(sprite.feet_x, candidate.Location.X);
                if(yDistance > 1.5 || overlap <= 0.0)
                    continue;
                if(!supportingBlock[i] || yDistance < bestDistance - 0.01 ||
                   (std::abs(yDistance - bestDistance) <= 0.01 && overlap > bestOverlap))
                {
                    supportingBlock[i] = &candidate;
                    bestDistance = yDistance;
                    bestOverlap = overlap;
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
            for(const Sprite& sprite : sprites)
                drawReceiverShadow(sprite, x, y, w, h, receiver, 0, srcY, shadow);
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
                        if(supportingBlock[i] == &block)
                            continue;
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
                drawClipped(mirrored, wx, wy, ww, wh, true,
                            XTColor(155, 195, 220, 110));
            }
        }
    }
}
