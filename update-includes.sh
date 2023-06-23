#!/bin/bash

set +ex

if [ ! "$ANDROID_BUILD_TOP" ]; then
    echo "lunch?"
    exit 1
fi

function update-includes() {
    find -L "$ANDROID_BUILD_TOP/hardware/libhardware/include/hardware" -maxdepth 1 -xtype l -exec rm {} \;

    for f in $ANDROID_BUILD_TOP/hardware/libhardware/include_all/hardware/*; do
        local bn="$(basename $f)"
        ln -s "../../include_all/hardware/$bn" "$ANDROID_BUILD_TOP/hardware/libhardware/include/hardware/$bn"
    done
}

update-includes
