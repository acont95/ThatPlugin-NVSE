# ThatPlugin NVSE

**ThatPlugin NVSE** is a plugin for **xNVSE (New Vegas Script Extender)** that introducing new features and bug fixes.

---

## Features

### Melee & Unarmed Ammo Usage (BETA)
- Enables **melee and unarmed weapons** to consume ammo, restoring the behavior Obsidian intended for melee weapons.
- Functionality of this feature is driven by assigning ammo to a weapon form of type melee or unarmed.
- Ammo effects and scripts are fully functional.
  
  Note ammo scripts require assigning a projectile to the weapon form. An assigned projectile is also necessary to ensure ammo effects apply when JIPs bIgnoreDTDRFix feature is enabled.
- Non automatic weapons consume ammo on hit with objects or actors.
- Automatic type weapons will consume ammo whenever firing.
- Reloads work as expected, though custom animations are required for most weapons.
- Empty case ejection should work for weapons with valid ShellCasingNode.

#### Planned/Future
- Fix for automatic melee weapons not stopping firing animation when ammo runs out. This is a vanilla bug with all weapon types.

---

## Installation

1. Ensure you have **xNVSE installed**.
2. Copy `ThatPlugin_NVSE.dll` (and any supporting files) into your `Data\NVSE\Plugins` folder.
3. Launch the game and verify the plugin loads in the NVSE log.

Mod manager is reccomended for install.

---

## Support & Contributions

- For bug reports, feature requests, or contributions, please open an issue or submit a pull request on the repository.

---

## License

## License

**ThatPlugin NVSE** is licensed under the **GNU General Public License (GPL) v3** or later.  
See the [LICENSE](LICENSE) file for full details.
