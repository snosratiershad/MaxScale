#!/bin/bash

if [ $# -lt 1 ]
then
    echo "Usage: $0 SRC"
    exit 1
fi

src=$1

if [ "$PWD" != "$src" ]
then
    # Copy sources to working directory
    cp -r -t $PWD/ $src/maxgui/
fi

cd $PWD/maxgui

# The pipefail option will cause the command to return the exit code of the
# first failing command in the pipeline rather than the default of returning the
# exit code of the last command in the pipeline.
set -o pipefail

# Piping the output through `tee` works around a problem in npm where it always
# prints verbose output: https://github.com/npm/cli/issues/3314
export buildPath=$PWD
export VUE_APP_GIT_COMMIT=$(cd $src && git rev-list --max-count=1 HEAD)
(npm ci --production --omit=optional && npm run build) |& tee
