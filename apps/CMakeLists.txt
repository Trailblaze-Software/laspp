set(LAS2LAS++_EXE_NAME las2las++)

add_executable(${LAS2LAS++_EXE_NAME} las2las++.cpp)

target_link_libraries(${LAS2LAS++_EXE_NAME} ${LIBRARY_NAME} OpenMP::OpenMP_CXX)

install(
  TARGETS ${LAS2LAS++_EXE_NAME}
  DESTINATION bin
  COMPONENT applications)
