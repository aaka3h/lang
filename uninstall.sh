#!/bin/bash
# Lang uninstaller

if [ -f "/usr/local/bin/lang" ]; then
    sudo rm /usr/local/bin/lang
    echo "Lang uninstalled."
else
    echo "Lang is not installed."
fi
