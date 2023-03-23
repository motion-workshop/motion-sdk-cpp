# Basic example, from the project README docs
add_executable(example_motionsdk example.cpp)
target_link_libraries(example_motionsdk PRIVATE motionsdk)

# More in depth example, stream to CSV format
add_executable(example_stream_csv example_stream_csv.cpp)
target_link_libraries(example_stream_csv PRIVATE motionsdk)
