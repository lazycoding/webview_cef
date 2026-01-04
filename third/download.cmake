set(download_path "https://github.com/hlwhl/webview_cef/releases/download/prebuilt_cef_bin_linux")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(cef_prebuilt_packages_name "cef_binary_114.2.14+gdc18c54+chromium-114.0.5735.91_linux_arm64.zip")
    else()
        set(cef_prebuilt_packages_name "cef_binary_114.2.9+g1a97a28+chromium-114.0.5735.91_linux_x64.zip")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(cef_prebuilt_packages_name "cef_binary_113.3.2+ge3ac123+chromium-113.0.5672.129_windows64.zip")

    # elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # message(STATUS "current system is MacOS")
    # set(cef_prebuilt_path "")
endif()

set(version_file_name "version.txt")
set(cef_prebuilt_path ${download_path}/${cef_prebuilt_packages_name})
set(cef_prebuilt_version_path ${download_path}/${version_file_name})

function(prepare_local_file filepath)
    if(NOT EXISTS ${filepath}/${cef_prebuilt_packages_name})
        message(STATUS "No local file ${cef_prebuilt_packages_name}, please run git lfs pull to download prebuilt.zip")
        return()
    endif()

    if(NOT EXISTS ${filepath}/../cef/version.txt)
        extract_file(${filepath}/${cef_prebuilt_packages_name} ${filepath}/../cef)
    endif()
endfunction(prepare_local_file)

function(extract_file filename extract_dir)
    message("${filename} will extract to ${extract_dir} ...")

    set(temp_dir ${CMAKE_BINARY_DIR}/tmp_for_extract.dir)

    if(EXISTS ${temp_dir})
        file(REMOVE_RECURSE ${temp_dir})
    endif()

    file(MAKE_DIRECTORY ${temp_dir})
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf ${filename}
        WORKING_DIRECTORY ${temp_dir})

    file(GLOB contents "${temp_dir}/*")
    list(LENGTH contents n)

    if(NOT n EQUAL 1 OR NOT IS_DIRECTORY "${contents}")
        set(contents "${temp_dir}")
    endif()

    get_filename_component(contents ${contents} ABSOLUTE)

    file(INSTALL "${contents}/" DESTINATION ${extract_dir})

    file(REMOVE_RECURSE ${temp_dir})
endfunction()

function(download_file url filename)
    message(WARNING "Downloading ${filename} from ${url}")
    file(DOWNLOAD ${url} ${filename} STATUS SHOW_PROGRESS)
endfunction(download_file)

function(prepare_prebuilt_files filepath)
    set(need_download FALSE)

    if(NOT EXISTS ${filepath})
        message(WARNING "No ${filepath} found")
        set(need_download TRUE)
    else()
        if(NOT EXISTS ${filepath}/version.txt)
            message(WARNING "No ${filepath}/version.txt found")
            set(need_download TRUE)
        else()
            download_file(${cef_prebuilt_version_path} ${filepath}/new_version.txt)
            file(READ ${filepath}/version.txt local_version)
            file(READ ${filepath}/new_version.txt new_version)

            if(local_version STREQUAL new_version)
                message(WARNING "No need to update ${filepath}")
            else()
                set(need_download TRUE)
            endif()

            file(REMOVE_RECURSE ${filepath}/new_version.txt)
        endif()
    endif()

    if(need_download)
        message(WARNING "Need to update ${filepath}")
        file(REMOVE_RECURSE ${filepath}/cmake ${filepath}/Debug ${filepath}/Release ${filepath}/Resources ${filepath}/libcef_dll ${filepath}/libcef_dll_wrapper)
        download_file(${cef_prebuilt_path} ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt.zip)
        file(MAKE_DIRECTORY ${filepath})
        extract_file(${CMAKE_CURRENT_SOURCE_DIR}/prebuilt.zip ${filepath})
        file(REMOVE_RECURSE ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt.zip)
    endif()
endfunction(prepare_prebuilt_files)
