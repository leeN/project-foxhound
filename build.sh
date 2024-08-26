#!/bin/bash

./mach build && \
./mach package && \
cd obj-tf-release/dist && \
rm -f foxhound_linux.zip && \
zip -r foxhound_linux.zip foxhound
