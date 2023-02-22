# Musical Floppy Drives

Repo containing a number of project files relating to a setup of musical floppy drives

## floppy_dds

Firmware for the Arduino Mega allowing control of up to 16 floppy drives. Accepts MIDI in. Any notes that cannot be allocated to a free drive will be forwarded out the MIDI OUT, allowing multiple Megas to be cascaded to control more than 16 drives with no code changes. Tone generation is performed using direct digital synthesis (DDS). Intended to be used with the `floppy_mega` breakout board.

## floppy_mega

Shield for the Arduino Mega that exposes 16 headers for connecting floppy drives. Intended to be paired with the `floppy_dds` firmware.

## floppy_bracket

PCB bracket designed to bind four floppy drives together. Two PCBs should be used.

## floppy_power

Power bus board