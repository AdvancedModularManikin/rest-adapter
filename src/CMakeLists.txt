#############################
# CMake REST Bridge root/src
#############################

#add_executable(amm_rest_adapter RESTAdapterMain.cpp DatabaseConnection.hpp DatabaseQueries.hpp ResourceManager.hpp)

add_executable(amm_rest_adapter RESTAdapterMain.cpp)

target_link_libraries(amm_rest_adapter
   PUBLIC amm_std
   PUBLIC sqlite3
   PUBLIC pistache
   PUBLIC atomic
   ${Boost_LIBRARIES}
   )

install(TARGETS amm_rest_adapter RUNTIME DESTINATION bin)
install(DIRECTORY ../config DESTINATION bin)
