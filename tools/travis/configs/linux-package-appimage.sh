#!/bin/sh

docker pull iscore/i-score-package-linux
docker run --name buildvm iscore/i-score-package-linux /bin/bash Recipe
docker cp buildvm:/i-score.AppImage i-score.AppImage
