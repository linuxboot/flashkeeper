# Soldered Chip Adapter

`wp_only_wide`: Write-protect-only chip adapter, controls flash chip write protect using switch or solder jumper, for wide (standard for flash chips) SOIC-8 and WSON-8.

`wp_only`: Same as above, for (rare for flash chips) "narrow" SOIC-8 packages

`breakout_wide`: Breakout chip adapter, for connecting your flash chip to external hardware (like a programmer or an SPIspy device) using a 0.5mm 8-pin FFC. For wide SOIC-8 and WSON-8.

`integrated_cable`: Experimental. Like the breakout adapter, but rather than having an FFC ZIF socket like the above, it has a short length of FFC built-in.

`adapter_soldered.kicad_pro` Overarching KiCAD project containing all variants. Use of this is optional.
