# Fork features

Differences between this fork ([deanxbox/PrismLauncher](https://github.com/deanxbox/PrismLauncher)) and upstream [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher). This is a personal build, not endorsed by the Prism Launcher team.

## Global options

Share one set of game options (mods, resource packs, shader pack selection) across instances.

- **Settings > Minecraft > Global Options:** an **Apply to all instances...** button pushes the global options to every instance.
- **Instance > Options tab:**
  - **Copy to Global** saves this instance's options as the global set (**Undo Copy to Global** reverts it).
  - **Sync Global Options** applies the global set to this instance.
  - **Restore Backup** restores the options as they were before the last sync.
- Mods that don't match the instance's Minecraft version or loader are skipped, and you get a warning listing them.
- Every apply asks for confirmation and makes a backup first, so it can be restored.

## Export list (Mods, Resource Packs, Shader Packs)

- The export action is available on all three pages, not just Mods. It exports the selected items, or everything if nothing is selected.
- New **Comma Separated List** format (e.g. `Mod1, Mod2`), saved as `.txt`.
- `{authors}` and `{mod_id}` are empty for non-mod resources.

## Resizable and reorderable columns (Mods, Resource Packs, Shader Packs)

- Every column can be dragged to resize, except the enable/disable checkbox column.
- The last visible column fills any leftover width.
- Columns can be dragged into a different order; the order is remembered per instance, and the checkbox column always stays first.

## Screenshots page

- Filter screenshots by date.
- GIF screenshots are shown (thumbnails and copy-image use the first frame; the date uses the file's modified time).

## Download mods dialog

- The "hide installed" filter and the "installed" badge also recognise a mod installed from the other platform (Modrinth or CurseForge), matched by file hash with a name/slug fallback.

## Keeping this list current

Add a section here when a feature is added to the fork. Run `git log --no-merges <upstream-merge-base>..HEAD` to see the fork's own commits.
