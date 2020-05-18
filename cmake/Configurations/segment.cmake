set(SCORE_SPLIT_DEBUG True)
include(travis/static-debug)

set(SCORE_PLUGINS_TO_BUILD
score-lib-inspector
score-lib-state
score-lib-device

score-plugin-inspector
score-plugin-deviceexplorer
score-lib-process
)

