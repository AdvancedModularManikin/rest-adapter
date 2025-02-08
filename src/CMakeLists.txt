#############################
# CMake REST Bridge root/src
#############################

add_executable(amm_rest_adapter
        main.cpp
        db/DatabaseMigrations.cpp
        api/Router.cpp
        api/endpoints/ActionEndpoints.cpp
        api/endpoints/AssessmentEndpoints.cpp
        api/endpoints/ModuleEndpoints.cpp
        api/endpoints/StatusEndpoints.cpp
        api/endpoints/SystemEndpoints.cpp
        utils/JsonUtils.cpp
        core/Config.cpp
        db/DatabaseConfig.cpp
        db/DatabaseConnection.cpp
        db/Queries.cpp
        db/QueryExecutor.cpp
        core/MoHSESManager.cpp
        core/MoHSESListener.cpp
        utils/ErrorHandler.cpp
        utils/Exceptions.cpp
        utils/FileUtils.cpp
        utils/HandlerWrapper.cpp
        utils/CustomHeaders.cpp
        services/ActionService.cpp
        services/AssessmentService.cpp
        services/ModuleService.cpp
        services/StatusService.cpp
        services/SystemService.cpp
)

target_link_libraries(amm_rest_adapter
   PUBLIC amm_std
   PUBLIC sqlite3
   PUBLIC pistache
   PUBLIC atomic

   )

install(TARGETS amm_rest_adapter RUNTIME DESTINATION bin)
install(DIRECTORY ../config DESTINATION bin)
