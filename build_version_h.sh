#!/bin/bash

# Format current date and time as yyyy-mm-dd hh:mm:ss
dt=$(date +"%Y-%m-%d %H:%M:%S")

# Create the version string
VERSION_STRING="V3.2 of $dt"

# Write the version string to src/build_version.h
echo "#define BUILD_VERSION \"$VERSION_STRING\"" > Src/build_version.h

# Write the version string to Objects/firmware_built.txt
echo "$VERSION_STRING" > Objects/firmware_built.txt
