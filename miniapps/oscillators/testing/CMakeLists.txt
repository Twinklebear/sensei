if (BUILD_TESTING)

  if (ENABLE_CATALYST)
    add_test(NAME testCatalystSlice
      COMMAND ${CMAKE_COMMAND}
      -DCATALYST_TEST_DRIVER:FILEPATH=$<TARGET_FILE:oscillator>
      -DIMAGE_TESTER:FILEPATH=$<TARGET_FILE:CompareImages>
      -DCATALYST_TEST_DIR:PATH=${CMAKE_BINARY_DIR}/Testing
      -DCATALYST_TEST_DATA=${CMAKE_CURRENT_SOURCE_DIR}
      -DMPIEXEC:FILEPATH=${MPIEXEC}
      -DMPIEXEC_NUMPROC_FLAG:STRING=${MPIEXEC_NUMPROC_FLAG}
      -DMPIEXEC_NUMPROCS=2
      -DMPIEXEC_PREFLAGS:STRING=${MPIEXEC_PREFLAGS}
      -DVTK_MPI_POSTFLAGS:STRING=${VTK_MPI_POSTFLAGS}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/CatalystSlice.cmake)
  endif()

endif()
