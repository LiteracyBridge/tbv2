#!/bin/sh

echo "Copy build artifacts to the Artifacts directory"

# Create the Artifacts directory if it doesn't exist
mkdir --parents Artifacts

# Regenerate .hex file
hex=TBookRev2b.hex
if [ -f "Objects/$hex" ]; then
    rm --verbose Objects/$hex
fi

arm-none-eabi-objcopy -O ihex out/TBook_Rev2b/TBv2b/TBook_Rev2b.axf Objects/$hex

# Copy build artifacts to the Artifacts directory
cp --verbose Objects/firmware_built.txt Artifacts/
cp --verbose Objects/$hex Artifacts/
