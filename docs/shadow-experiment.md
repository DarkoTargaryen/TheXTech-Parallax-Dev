# Shadow and reflection experiment — alpha-mask revision

- **Windows CI:** x64 only on the `shadow-experiment` branch. Other branches are unchanged.
- **Shadow casters:** players, NPCs (including vines), and ordinary blocks.
- **Shadow receivers:** background objects (BGOs) and resizable blocks only.
- **Excluded:** normal terrain, Background2/parallax, water and quicksand never receive a shadow.
- **Masking:** OpenGL shader intersects the caster sprite alpha/legacy GIF mask with the BGO or resizable-block PNG alpha/legacy GIF mask. No image files need editing.
- Resizable blocks use their three-by-three tile source texture when sampling transparency.
- Water reflections remain a separate experimental effect; quicksand is excluded from reflections.
- If a renderer lacks the shader, it intentionally omits shadows instead of showing shadows through transparent rectangles.
- The branch is experimental; validate x64 CI and visually test custom PNGs, GIF bitmasks, sizable blocks, foreground BGO sorting and performance before merging into `parallax-dev`.
