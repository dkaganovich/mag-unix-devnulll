#!/bin/bash

echo 'SUBSYSTEM=="misc", KERNEL=="nulll", MODE="0666"' > /lib/udev/rules.d/99-nulll.rules
