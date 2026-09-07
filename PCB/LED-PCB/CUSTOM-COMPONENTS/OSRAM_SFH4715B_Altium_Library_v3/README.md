# OSRAM SFH 4715B Altium library v3

Import `OSRAM_SFH4715B_Altium_Import_v3.lbr` with **File > Import Wizard > EAGLE Projects and Designs**.

Version 3 corrects the Altium pad designators:

| Physical land | Altium pad | Datasheet electrical function | Typical circuit net |
|---|---:|---|---|
| Upper 2.80 x 0.55 mm | 1 | Cathode, K | GND or a switched current-sink node |
| Center 2.80 x 1.00 mm | 2 | Anode and primary thermal path | Same anode/VCC net as the lower land |
| Lower 2.80 x 0.55 mm | 2 | Anode, A | VCC/anode supply |

The center land is thermal in its mechanical purpose, but it is not an electrically isolated thermal pad. OSRAM's package drawing labels both the center and lower contacts as **anode**. Therefore, both physical lands intentionally use pad designator `2`. Altium assigns both to schematic pin 2 and to the same net.

Do not create an independent `THERMAL` net for the center pad. If the LED anode is connected to `VCC`, the center thermal land must also be `VCC`. Add same-net copper around the anode lands for heat spreading according to the available board area.

The cathode is not inherently GND. If the LED is driven by a low-side MOSFET or constant-current sink, cathode pin 1 belongs to that switched node. It should be called GND only if it is actually wired directly to ground in the schematic.

## Import and replacement

1. Remove/uninstall the v2 integrated library.
2. Import the v3 `.lbr`.
3. Install the generated v3 `.IntLib`.
4. On the schematic, run **Tools > Update From Libraries** with **Update models** enabled, or delete and replace the old LED symbols from the v3 library.
5. Run **Design > Update PCB Document**.
6. In the PCB, confirm the upper land reports pad `1` and both center/lower lands report pad `2`.

If the EAGLE importer renames duplicate pad designators, use the files under `KiCad_Fallback` with Altium's **KiCad Design Files** Import Wizard. The KiCad footprint natively contains one pad `1` and two pads `2`, and both the symbol and footprint are named `SFH4715B`.

## Verified geometry

- Body: 3.75 x 3.75 mm
- Lens: 2.76 mm diameter
- Maximum height: 2.29 mm
- Copper: 2.80 mm wide; outer lands 0.55 mm high; center land 1.00 mm high
- Top paste: four explicit apertures, including the split center aperture
- Top solder mask: three exact openings matching the copper lands
- Source land pattern: OSRAM `E062.3010.91-06`
- Source package drawing: OSRAM `C63062-A4141-A1...-12`
