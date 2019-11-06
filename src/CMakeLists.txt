#############################
# CMake REST Bridge root/src
#############################

set(REST_BRIDGE_SOURCES RESTBridgeMain.cpp)

add_executable(amm_rest_bridge ${REST_BRIDGE_SOURCES})

target_link_libraries(amm_rest_bridge
        PUBLIC amm_std
        PUBLIC sqlite3
	PUBLIC pistache
	PUBLIC atomic
        ${Boost_LIBRARIES}
        )

install(TARGETS amm_rest_bridge RUNTIME DESTINATION bin)
install(DIRECTORY ../config DESTINATION bin)
