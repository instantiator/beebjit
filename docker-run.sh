#!/bin/bash

set -e
set -o pipefail

./docker-build.sh
docker run -it beebjit
