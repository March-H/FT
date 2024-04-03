add_library(scheduler scheduler.cpp Job.h Job.cpp config.h List.h Channel.h)
target_link_libraries(scheduler logger)
target_compile_options(scheduler PRIVATE -fPIC)