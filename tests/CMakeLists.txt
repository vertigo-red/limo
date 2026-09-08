# catch2
find_package(Catch2 3 REQUIRED)

set(TEST_SOURCES
        test_applicationmanager.cpp
        test_backupmanager.cpp
        test_backuplistmodel.cpp
        test_bg3deployer.cpp
        test_conflictsmodel.cpp
        test_cryptography.cpp
        test_deployer.cpp
        test_deployerlistmodel.cpp
        test_fomodinstaller.cpp
        test_installer.cpp
        test_lootdeployer.cpp
        test_lspakextractor.cpp
        test_moddedapplication.cpp
        test_openmwdeployer.cpp
        test_reversedeployer.cpp
        test_rootlevelcondition.cpp
        test_tagconditionnode.cpp
        test_modlistmodel.cpp
        test_tool.cpp
        test_utils.cpp
        test_utils.h
        tests.cpp
)

configure_file(test_utils.h.in test_utils.h)

add_executable(tests ${TEST_SOURCES})
target_include_directories(tests
    PRIVATE core
    PRIVATE ${PROJECT_SOURCE_DIR}/src
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui
)
target_sources(tests
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/applicationmanager.cpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/applicationmanager.h
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/backuplistmodel.cpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/backuplistmodel.h
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/conflictsmodel.cpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/conflictsmodel.h
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/deployerlistmodel.cpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/deployerlistmodel.h
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/deployerlistproxymodel.cpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/deployerlistproxymodel.h
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/modlistmodel.cpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/modlistmodel.h
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/modlistproxymodel.cpp
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/modlistproxymodel.h
    PRIVATE ${PROJECT_SOURCE_DIR}/src/ui/rootlevelcondition.cpp
)
target_link_libraries(tests
    PRIVATE Catch2::Catch2WithMain
    PRIVATE Qt5::Widgets
    PRIVATE core
)

include(Catch)
catch_discover_tests(tests)
