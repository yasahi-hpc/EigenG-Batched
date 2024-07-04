add_library(EigenG_Batched)
target_sources(EigenG_Batched PRIVATE eigen_GPU_check.cpp eigen_GPU_batch.cpp)

target_include_directories(EigenG_Batched INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})