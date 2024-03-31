set(scripts_dir ${CMAKE_SOURCE_DIR}/scripts)
set(git_hash_dir ${CMAKE_SOURCE_DIR}/main)

function(GetGitHash)
    execute_process(
        COMMAND python ${scripts_dir}/githash.py ${git_hash_dir}/githash.h
    )
endfunction()