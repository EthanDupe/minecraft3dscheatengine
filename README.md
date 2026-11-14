# MC3DS Cheat Engine (.3gx)

A `CTRPluginFramework` cheat engine for *Minecraft: New Nintendo 3DS Edition* (v1.9.19), built with verified memory offsets from [Minecraft-3DS-Community/GamePatches](https://github.com/Minecraft-3DS-Community/GamePatches) and the [PabloMK7/CTRPluginFramework-BlankTemplate](https://github.com/PabloMK7/CTRPluginFramework-BlankTemplate) toolchain.

## Building on GitHub Actions
1. Create a new GitHub repository and upload the contents of this archive (including the hidden `.github/` folder).
2. Open the **Actions** tab and run **Build MC3DS Cheat Engine (.3gx)** (or push a commit).
3. Download the compiled `.3gx` file from the workflow **Artifacts** section.

## Installing on Nintendo 3DS (Luma3DS)
Copy the `.3gx` file to your microSD card for your game region:
* **USA (`000400000017FD00`):** `sdmc:/luma/plugins/000400000017FD00/mc3ds_cheat_engine.3gx`
* **EUR (`00040000001B8700`):** `sdmc:/luma/plugins/00040000001B8700/mc3ds_cheat_engine.3gx`

Open the Rosalina Menu (`L + D-Pad Down + Select`), set **Enable plugin loader** to `[Enabled]`, and launch *Minecraft: New Nintendo 3DS Edition*.
* Press `L + R + D-Pad Down` for the bottom-screen panel.
* Press `Select` for the full `CTRPluginFramework` menu.
