[1mdiff --git a/CMake/IScoreFunctions.cmake b/CMake/IScoreFunctions.cmake[m
[1mindex 5eb45b1..0a6a1af 100644[m
[1m--- a/CMake/IScoreFunctions.cmake[m
[1m+++ b/CMake/IScoreFunctions.cmake[m
[36m@@ -83,8 +83,8 @@[m [mfunction(setup_iscore_plugin PluginName)[m
 [m
   if(ISCORE_BUILD_FOR_PACKAGE_MANAGER)[m
   install(TARGETS "${PluginName}"[m
[31m-          LIBRARY DESTINATION bin/i-score-plugins[m
[31m-          ARCHIVE DESTINATION bin/i-score-plugins[m
[32m+[m[32m          LIBRARY DESTINATION lib/i-score[m
[32m+[m[32m          ARCHIVE DESTINATION lib/i-score[m
           COMPONENT DynamicRuntime)[m
   else()[m
   install(TARGETS "${PluginName}"[m
[1mdiff --git a/base/app/i-score.sh b/base/app/i-score.sh[m
[1mindex d3a4f59..97340b1 100755[m
[1m--- a/base/app/i-score.sh[m
[1m+++ b/base/app/i-score.sh[m
[36m@@ -1,2 +1,2 @@[m
 #!/bin/sh[m
[31m-LD_LIBRARY_PATH=.:i-score-plugins ./i-score[m
[32m+[m[32mLD_LIBRARY_PATH=.:/usr/lib/i-score ./i-score[m
