if (ENABLE_SENSEI)
  message(STATUS "Enabled: sensei library")

  set(sensei_sources AnalysisAdaptor.cxx Autocorrelation.cxx
    ConfigurableAnalysis.cxx DataAdaptor.cxx DataRequirements.cxx
    Histogram.cxx Error.cxx ProgrammableDataAdaptor.cxx VTKHistogram.cxx
    VTKDataAdaptor.cxx VTKUtils.cxx)

  set(sensei_libs mpi pugixml vtk thread ArrayIO timer diy grid)

  if(ENABLE_CATALYST)
    list(APPEND sensei_sources CatalystAnalysisAdaptor.cxx
      CatalystSlice.cxx CatalystUtilities.cxx)
  endif()

  if(ENABLE_ADIOS)
    list(APPEND sensei_sources ADIOSSchema.cxx
      ADIOSAnalysisAdaptor.cxx ADIOSDataAdaptor.cxx)
    list(APPEND sensei_libs adios)
  endif()

  if(ENABLE_VTK_M)
    list(APPEND sensei_sources
      VTKmContourAnalysis.cxx
      VTKmContourAnalysis.h)
  endif()

  if(ENABLE_LIBSIM)
    list(APPEND sensei_sources LibsimAnalysisAdaptor.cxx
      LibsimImageProperties.cxx)
    list(APPEND sensei_libs libsim)
  endif()

  if(ENABLE_VTK_IO)
    list(APPEND sensei_sources VTKPosthocIO.cxx)
    if (ENABLE_VTK_MPI)
      list(APPEND sensei_sources VTKAmrWriter.cxx)
    endif()
  endif()

  add_library(sensei ${sensei_sources})

  target_include_directories(sensei PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)

  target_link_libraries(sensei PUBLIC ${sensei_libs})

  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
    DESTINATION include FILES_MATCHING PATTERN "*.h")

  install(TARGETS sensei EXPORT sensei INCLUDES DESTINATION include
    ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)

  install(EXPORT sensei DESTINATION lib/cmake
    EXPORT_LINK_INTERFACE_LIBRARIES)

  add_subdirectory(testing)
endif()
