
#定义了一个SRC_LIST变量，包含了当前目录下的所有源文件
aux_source_directory(. SRC_LIST)


#生成可执行文件
add_executable(ChatClient ${SRC_LIST})

#执行可执行文件所依赖的库
target_link_libraries(ChatClient pthread)