# Get the absolute path of the directory that contains this component
get_filename_component(this_cmp_root_dir ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
# dir for python stuff
set(working_dir ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/netlogging.dir)

# Get the python interpreter to use for running the scripts
if(COMMAND idf_build_get_property)
    idf_build_get_property(python PYTHON)
else()
    set(python python)
endif()

# Install any requirements for this component.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_custom_command(OUTPUT ${working_dir}/requirements.stamp
        DEPENDS ${this_cmp_root_dir}/requirements.txt
        COMMAND ${CMAKE_COMMAND} -E make_directory ${working_dir}
        COMMAND ${python} -m pip install -r ${this_cmp_root_dir}/requirements.txt
        COMMAND ${CMAKE_COMMAND} -E touch ${working_dir}/requirements.stamp
    )
endif()



function(target_add_netlogging_asset target asset_src)
    set (asset_dst_dir "${CMAKE_BINARY_DIR}/www")
    set (asset_dst "${asset_dst_dir}/index.html.gz")
    add_custom_command(
        WORKING_DIRECTORY ${COMPONENT_DIR}
        OUTPUT ${asset_dst}
        COMMAND ${CMAKE_COMMAND} -E echo "Compressing static assets for built-in HTTP SSE Logging Server..."
        COMMAND ${python} ${this_cmp_root_dir}/tools/compress_assets.py --outdir=${asset_dst_dir} ${asset_src}
        DEPENDS ${asset_src}
    )
    add_custom_target(builtin_sse_assets DEPENDS "${asset_dst}")
    target_add_binary_data(${target} "${asset_dst}" BINARY)
endfunction()
