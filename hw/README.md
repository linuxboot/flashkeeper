# Flashkeeper Hardware
For source of the Flashkeeper website, see https://github.com/linuxboot/flashkeeper.

`adapter_soldered`: KiCAD files for the soldered Flashkeeper chip adapter

`dummy_board`: Rigid PCB with multiple flash package footprints and connectors for testing and development

`spispy_board`: FPGA-SoM-based board to run `spispy` during development

`single_stencil`: Project containing all PCBs in a single file. Intended to export a single paste-layer Gerber so that all Flashkeeper boards can be made using different parts of a single solder paste stencil.

`CAD`: FreeCAD files for Flashkeeper mechanical components

`flashkeeper.kicad_sym`: Symbol library for components used in Flashkeeper

`flashkeeper.pretty`: Footprint library for components used in Flashkeeper

KiCAD and FreeCAD files are for the latest upstream versions of KiCAD and FreeCAD, as of the last modification of that file. Depending on which versions your OS's packages ship, on some platforms (like Debian), this may mean installing from another repository (e.g. Debian Backports), or using an alternative packaging system such as Flatpak or AppImage. Please follow the installation instructions for your platform here:

- https://www.kicad.org/download/

- https://www.freecad.org/downloads.php
