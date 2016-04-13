include(debug)
set(ISCORE_STATIC_QT True)
#~ LTO doesn't seem to work sadly on current ubuntu
#~ e.g. lto1: internal compiler error: in add_symbol_to_partition_1, at lto/lto-partition.c:158
if(UNIX AND NOT APPLE)
    set(ISCORE_ENABLE_LTO False)
endif()
