# Run after macdeployqt: cmake -DBUNDLE=<path to .app> -P StripAbsoluteRpaths.cmake
#
# macdeployqt needs the build-tree LC_RPATH entries to find and bundle dependencies such as
# libde265, but it leaves them in the copied libraries, and macOS's pre-distribution check
# rejects any rpath that points outside the bundle. The bundle resolves @rpath through the
# executable's @executable_path/../Frameworks, so the absolute entries can simply go.

set(_changed FALSE)
file(GLOB_RECURSE _libs "${BUNDLE}/Contents/Frameworks/*.dylib" "${BUNDLE}/Contents/PlugIns/*.dylib")
foreach(_lib IN LISTS _libs)
    execute_process(COMMAND otool -l "${_lib}" OUTPUT_VARIABLE _loadCommands COMMAND_ERROR_IS_FATAL ANY)
    string(REGEX MATCHALL "path /[^\n]* \\(offset" _absolute "${_loadCommands}")
    foreach(_entry IN LISTS _absolute)
        string(REGEX REPLACE "^path (/[^\n]*) \\(offset$" "\\1" _rpath "${_entry}")
        execute_process(COMMAND install_name_tool -delete_rpath "${_rpath}" "${_lib}" COMMAND_ERROR_IS_FATAL ANY)
        set(_changed TRUE)
    endforeach()
endforeach()

# install_name_tool voids signatures and arm64 macOS refuses to load unsigned code, so
# re-seal the bundle ad hoc; release builds are re-signed with the real identity in CI.
if(_changed)
    execute_process(COMMAND codesign --force --deep --sign - "${BUNDLE}" OUTPUT_QUIET ERROR_QUIET COMMAND_ERROR_IS_FATAL ANY)
endif()
