# Shadow and reflection prototype

This branch is isolated from `parallax-dev` and is not a finished effect. **Do not merge** until its visuals are reviewed in-game.

## Implemented in this experiment
- Automatic silhouettes based on existing player, NPC (including vines), and ordinary block graphics.
- Hard-edged, 8px down/right shadows, approximately 40% alpha.
- Shadows are restricted to BGO and resizable-block rectangular bounds.
- Transparent sprite pixels remain transparent; no graphic assets are modified.
- Water-region reflection preview mirrors sprite textures vertically into non-quicksand water areas and clips to water bounds.
- Shared-screen rendering uses the current camera offsets. No physics changes.

## Important limitations
- **Shadow receivers are not yet per-pixel alpha masked.** Shadows may show through transparent gaps within BGO or resizable-block sprites.
- Sprite-based reflection does not capture all game layers, mounts, or parallax. No ripple distortion.
- Shadows/reflections draw in the layer after normal BGOs, before standard NPC/player draws; foreground-BGO layering is incomplete.
- Shadows from ordinary blocks are approximated from the block's sprite frame.
- Windows CI compilation still needs checking. In-game correctness and performance have not been verified.

Existing `parallax-dev` commits and Windows executables remain unchanged.
