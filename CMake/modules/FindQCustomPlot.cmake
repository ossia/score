# - Try to find QCustomPlot
# Once done this will define
#  QCustomPlot_FOUND - System has QCustomPlot
#  QCustomPlot_INCLUDE_DIRS - The QCustomPlot include directories
#  QCustomPlot_LIBRARIES - The libraries needed to use QCustomPlot
#  QCustomPlot_DEFINITIONS - Compiler switches required for using QCustomPlot

find_package(PkgConfig)
pkg_check_modules(PC_QCustomPlot QUIET qcustomplot)
set(QCustomPlot_DEFINITIONS ${PC_QCustomPlot_CFLAGS_OTHER})

find_path(QCustomPlot_INCLUDE_DIR qcustomplot.h
		  HINTS ${PC_QCustomPlot_INCLUDEDIR} ${PC_QCustomPlot_INCLUDE_DIRS})

find_library(QCustomPlot_LIBRARY NAMES qcustomplot
			 HINTS ${PC_QCustomPlot_LIBDIR} ${PC_QCustomPlot_LIBRARY_DIRS} )

set(QCustomPlot_LIBRARIES ${QCustomPlot_LIBRARY} )
set(QCustomPlot_INCLUDE_DIRS ${QCustomPlot_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(QCustomPlot
								  DEFAULT_MSG
								  QCustomPlot_LIBRARY
								  QCustomPlot_INCLUDE_DIR)

mark_as_advanced(QCustomPlot_INCLUDE_DIR QCustomPlot_LIBRARY)
