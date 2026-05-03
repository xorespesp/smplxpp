set(TARGET_NAME cnpy)
add_library(${TARGET_NAME} STATIC)

target_sources(${TARGET_NAME}
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src/cnpy.cpp"
)

target_include_directories(${TARGET_NAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/cnpy>
)

include(FetchZlib)
target_link_libraries(${TARGET_NAME}
	PUBLIC
		zlibstatic
)

# Install cnpy headers
install(
	DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/cnpy"
	FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)